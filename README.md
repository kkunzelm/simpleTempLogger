# simpleTempLogger

Temperature logger based on an ESP 8266 node delivering MQTT messages via Wifi. This sketch goes in deep sleep mode once the message string has been sent to the MQTT broker and wakes up periodically to repeat all steps.

* The MQTT message contains
  * the name of the influxdb database, i.e. telemetrie
  * location of the sensor: qth, see https://en.wikipedia.org/wiki/Maidenhead_Locator_System
  * sensor typ, i.e. DS18B20
  * sensorID, i.e. plain number, like 1, 2, 3 etc. or any descriptive string
  * temperature (or any other sensor value like humidity, air pressure etc. depending on sensors used)
* indirect measure of battery voltage. If the voltage is below a threshold level, the sensor stays in deep sleep until brown out occurs.
* unix time stamp as retrieved by the MCU client from a ntp server

Example of the current MQTT message format: 

    telemetrie,qth=JN58SD,sensor=DS18B20,number=2 temperature=22.87,voltage=3475 1579438238000000000
