/* The code is the documentation. To save myself time when I have to modify the code in the future
 * I will comment the code "vv" (= very verbose)! 
 * While this code uses much more strings then necessary it is easy to read and modify.
 *
 * Hardware: Wemos D1 mini V3.1 -> https://www.wemos.cc/en/latest/d1/d1_mini.html)
 * Last software modification: Jan 2020
 * Autor: KHK
 * 
 * Code based on:
 *     http://www.jerome-bernard.com/blog/2015/10/04/wifi-temperature-sensor-with-nodemcu-esp8266/
 *     and other internet sources.
 * 
 * Purpose: Sketch which publishes temperature data from a DS1820 sensor to a MQTT topic.  
 *
 *     Current message format: 
 *           telemetrie,qth=JN58SD,sensor=DS18B20,number=2 temperature=22.87,voltage=3475 1579438238000000000
 *
 *     The MQTT message contains
 *     - the name of the influxdb database, i.e. telemetrie
 *     - qth, see https://en.wikipedia.org/wiki/Maidenhead_Locator_System
 *     - sensortyp, i.e. DS18B20
 *     - sensorID, i.e. plain number, like 1, 2, 3 etc.
 *     - temperature (, humidity, etc depending on sensor)
 *     - indirect measure of battery voltage
 *     - unix time in ns as retrieved by the MCU client from a ntp server  
 *     - This sketch goes in deep sleep mode once the message string has been sent to the MQTT
 *       topic and wakes up periodically (configure SLEEP_DELAY_IN_SECONDS accordingly).
 *
 * Hookup guide:
 *   - to wake up from deep sleep: 
 *     connect D0 (GPIO16) pin to RST pin in order to enable the ESP8266 to wake up periodically.
 *     GPIO16 is connected to the internal RTC - real time clock, 
 *     which pulls the RST pin low when the timer has run down. 
 *     The D0/RST connection must be either removed for programming or 
 *     as an alternative approach: push the reset pin AND start uploading immediately
 *   - for DS18B20:
 *      + connect VCC (3.3V) to the appropriate DS18B20 pin (Vdd)
 *      + connect GND to the appopriate DS18B20 pin (GND)
 *      + connect D3 to the DS18B20 data pin (DQ) 
 *        annotation KHK: if you connect it to D4 as usually suggested the onboard LED will blink synchronous to 
 *                      every transmission. This is disturbing sometimes. Therefore I explicitely disable the 
 *                      onboard LED.
 *      + connect a 4.7K resistor between DQ and VCC.
 *    - Wemos D1 mini: see schematic here https://docs.wemos.cc/en/latest/_static/files/sch_d1_mini_v3.0.0.pdf
 *      cut the trace between R2 and R1 to allow a floating ADC, the cut removes R2 from ADC 
 *      the voltage divider from A0 to GND for ADC will no longer work!
 *    - Power from LiPo 18650 cell: connect GND to GND, connect the positive wire of the battery to +5V of D1 mini
 *      The +5V is before the onboard LDO thus the ESP8266 is powered from stabilized 3.3V.
 *
 * KHK additional comments: 
 *     - uses input pin D3 for DS18B20 so that the LED_BUILTIN can be disabled
 *       monitors internal voltage (https://electronza.com/esp8266-running-on-battery-power/)
 *       no voltage divider is used to monitor battery voltage to keep continous power consumption low 
 *     - added indirect voltage measurement to prepare for battery powering
 *     - added NTP service to identify duplicate messages in influxdb
 *     - use influxdb protocol format for MQTT message
 *     
 */

#include <ESP8266WiFi.h>             // wifi

#include <NTPClient.h>               // ntp
#include <WiFiUdp.h>                 // ntp

#include <PubSubClient.h>            // mqtt

#include <OneWire.h>                 // DS18B20
#include <DallasTemperature.h>       // DS18B20

#define SLEEP_DELAY_IN_SECONDS  30   // [s], the maximum sleep time is 4294967295 us, ~71 minutes.
#define ONE_WIRE_BUS            D3   // DS18B20 data pin

// WIFI credentials
const char* ssid = "XXXXXXXXXXXX";    
const char* password = "XXXXXXXXXXXX";

// Address of MQTT Broker
const char* mqtt_server = "XXXXXXXXXXXX"; //MQTT_BROKER_IP_ADDRESS

/* By default, Mosquitto will still allow anonymous connections, 
 * i.e. connections where no username/password is provided. 
 * In addition to the password_file entry, you also need: allow_anonymous false
 */
const char* mqtt_username = "";
const char* mqtt_password = "";
const char* mqtt_topic = "sensors";     // sensors is just an example

// individual MQTT message components

String databasename = "telemetrie";     // name of the influxdb database, telemetrie is just an example
String qthlocator = "JN58SD";           // JN58SD = München, just an example
String sensortyp = "DS18B20";
String sensorID = "2";                  // 2 is just an example
String clientID = qthlocator + sensortype + sensorID;      // this clientID is just an example
String measuredProperty1 = "temperature";
String measuredProperty2 = "voltage";

// Text modules to compose MQTT message in influxdb line format 
// (https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/)
// String influxdbmessage1 = "telemetrie,qth=JN58SD,sensor=DS18B20,number=2 temperature=";
String influxdbmessage1 = databasename + 
                          ",qth="+
                          qthlocator+
                          ",sensor="+
                          sensortyp+
                          ",number="+
                          sensorID+
                          " "+
                          measuredProperty1+
                          "=";

// String influxdbmessage2 = ",voltage="; 
String influxdbmessage2 = ","+
                          measuredProperty2+
                          "=";

// ------------ nothing to edit beyond this point ------------

// Strings of numerical data for MQTT message
char temperatureString[6];
char voltageString[6];

// Monitoring Vin without external parts
// from https://electronza.com/esp8266-running-on-battery-power/
ADC_MODE(ADC_VCC);    // to check battery voltage
int Batt;             // to check battery voltage

const long utcOffsetInSeconds = 0;       // offset for ntp time zone, 0 = we stay with UTC

// Define NTP Client to get time
WiFiUDP ntpUDP;

/* By default 'pool.ntp.org' is used with 60 seconds update interval and no offset
 * You can specify the time server pool and the offset, (in seconds)
 * Additionaly you can specify the update interval (in milliseconds).
 * NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
 */

// Berlin: UTC+01:00  --> 01:00 is in h, for s use: 3600 
// It is good practice to use UTC, however!
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);

WiFiClient espClient;
PubSubClient client(espClient);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

// ********************************************************************************

void setup() {
  
       // Check battery status (https://electronza.com/esp8266-running-on-battery-power/)
       //     Reads Vin (not battery voltage!!! this is the voltage after the LDO)
       //     But Vin = battery voltage IF battery_voltage < 3.3V -> see datasheet of LDO

       Batt = ESP.getVcc();
       delay(100);

       // If the battery is discharged sleep as long as possible but don't go any further along this program!!!
       if(Batt < 3100){
          // Deep sleep for as long as you can
          ESP.deepSleep(ESP.deepSleepMax());
          delay(100);
       }
         
       // setup serial port
       Serial.begin(115200);

       // to make sure the LED is not consuming power
       // initialize digital pin LED_BUILTIN as an output
       pinMode(LED_BUILTIN, OUTPUT);
       digitalWrite(LED_BUILTIN, HIGH);   // turn the LED off -> Wemos D1 mini LED logic is inverted

       // output the battery value (https://electronza.com/esp8266-running-on-battery-power/)
       Batt = ESP.getVcc();
       Serial.println("Vin voltage is: ");  
       Serial.println(Batt);

       // setup WiFi
       setup_wifi();

       timeClient.begin();
         
       client.setServer(mqtt_server, 1883);
       client.setCallback(callback);

       // setup OneWire bus
       DS18B20.begin();
}

void setup_wifi() {
       delay(100);
       // We start by connecting to a WiFi network
       Serial.println();
       Serial.print("Connecting to ");
       Serial.println(ssid);

       WiFi.begin(ssid, password);

       while (WiFi.status() != WL_CONNECTED) {
           delay(500);
       Serial.print(".");
        }

       Serial.println("");
       Serial.println("WiFi connected");
       Serial.println("IP address: ");
       Serial.println(WiFi.localIP());
}

// KHK currently not used
void callback(char* topic, byte* payload, unsigned int length) {
       Serial.print("Message arrived [");
       Serial.print(topic);
       Serial.print("] ");
       for (int i = 0; i < length; i++) {
              Serial.print((char)payload[i]);
       }
       Serial.println();
}

void reconnect() {
       // Loop until we're reconnected
       while (!client.connected()) {
              Serial.print("Attempting MQTT connection...");
              // Attempt to connect
              if (client.connect(clientID, mqtt_username, mqtt_password)) {
                     Serial.println("connected");
              } else {
                     Serial.print("failed, rc=");
                     Serial.print(client.state());
                     Serial.println(" try again in 1 second");
                     // Wait 1 seconds before retrying
                     delay(1000);
              }
       }
}

float getTemperature() {
       Serial.println("Requesting DS18B20 temperature...");
       float temp;
       do {
              DS18B20.requestTemperatures(); 

              // Plausibility check for temperature values
              if(DS18B20.getTempCByIndex(0) > -50 && DS18B20.getTempCByIndex(0) < 150){
                  temp = DS18B20.getTempCByIndex(0);
              }
              delay(1000);

       } while (temp == 85.0 || temp == (-127.0));     // The power-on reset value of the temperature register is +85°C.
                                                       // Error Code -127: #define DEVICE_DISCONNECTED_C -127
       return temp;
}

// ********************************************************************************

void loop() {
     
      if (!client.connected()) {
        reconnect();
      }
      client.loop();
    
      // send data to the MQTT topic
      // The publish functions expect char[] types to be passed in rather than Strings.
      // You need to use the String.toCharArray() function to convert your strings to the necessary type. Some say
      // that .c_str.() is the more elegant alternative 
      // (https://arduino.stackexchange.com/questions/518/is-it-better-to-use-c-str-or-tochararray)

      float temperature = getTemperature();
      // convert temperature to a string with two digits before the comma and 2 digits for precision
      dtostrf(temperature, 2, 2, temperatureString);

      // convert battery readings to string
      dtostrf(Batt, 2, 0, voltageString);
      
      timeClient.update();

      // compose MQTT message string
      // influxdb expects timestamps in [ns]
      // test with https://www.epochconverter.com/
      String message = influxdbmessage1 + 
                       temperatureString + 
                       influxdbmessage2 + 
                       voltageString + " " + 
                       String(timeClient.getEpochTime()) + "000000000";  
      
      Serial.println(message);
     
      client.publish(mqtt_topic, message.c_str());
    
      Serial.println("Closing MQTT connection...");
      client.disconnect();
    
      Serial.println("Closing WiFi connection...");
      WiFi.disconnect();
    
      delay(100);
    
      Serial.print("Entering deep sleep mode for ");
      Serial.print(SLEEP_DELAY_IN_SECONDS);
      Serial.println(" s");

      ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_RF_DEFAULT);  
      // KHK to save even more power use: WAKE_NO_RFCAL
      // thus you disable wifi calibration when you return from deep sleep, 
      // you will avoid a power surge at this time 
      // example call: ESP.deepSleep(SLEEP_DELAY_IN_SECONDS * 1000000, WAKE_NO_RFCAL);
      delay(500);   // wait for deep sleep to happen
}
