# simpleTempLogger

## Software

Temperature logger based on an ESP 8266 node delivering MQTT messages via WiFi. This sketch goes in deep sleep mode once the message string has been sent to the MQTT broker and wakes up periodically to repeat all steps.

* The MQTT message contains
  * the name of the influxdb database, i.e. telemetrie
  * location of the sensor: qth, see https://en.wikipedia.org/wiki/Maidenhead_Locator_System
  * sensor typ, i.e. DS18B20
  * sensorID, i.e. plain number, like 1, 2, 3 etc. or any descriptive string
  * temperature (or any other sensor value like humidity, air pressure etc. depending on sensors used)
* indirect measure of battery voltage. If the voltage is below a threshold level, the sensor stays in deep sleep until brown out occurs.
* unix time stamp as retrieved by the MCU client from a ntp server

Example of the current MQTT message format (InfluxDB line protocol format): 

    telemetrie,qth=JN58SD,sensor=DS18B20,number=2 temperature=22.87,voltage=3475 1579438238000000000


## Hardware

The code was implemented on a Wemos D1 mini (V 3.1), which is fed by a single 18650 lipo-cell. The entire assembly was mounted in a 3D printed housing. The battery contacts have the dimensions 18.5 mm x 16 mm x 0.3 mm. These contacts are relatively easy to find and buy online. 

The idea for the case came from http://www.thingiverse.com/thing:3116091. 

But I wanted to use the case as a freestanding indoor version. For this reason some modifications were necessary, so I redrew the case completely in Solidworks. 

Personally I think the idea of publishing stl files on Thingiverse is very good. Less good is that the original files of the CAD programs are not included. 

To make it easier for others to modify the case, I have archived my Solidworks files here. The files are also available in STL format. 

In my experience it would be sufficient to know some dimensions exactly. If you know your CAD program well enough, it is usually faster to redraw the parts anyway than to make changes to other CAD drawings. For this reason I have also added PDF files with the most important dimensions. 

The numerous holes on the mounting side of the battery compartment allow to mount different microcontrollers (e.g. Wemos D1 mini, Arduino nano, Adafruit Feather M4 or a 30 mm x 70 mm breadboard).

For this project I decided to make everything as simple as possible (KISS). For this reason I soldered all connections directly and did not use any plug contacts. The microcontroller was mounted on the battery compartment with self-tapping wood screws (SPAX, 3.0 x 12 mm) and the battery compartment was screwed to the base plate with self-tapping wood screws (SPAX 2.5 x 9 mm). 

## Documentation

You find a minimal documentation, how to setup a server to display your data based on the programs:

* Mosquitto -> Mosquitto.mkd
* Telegraf -> Telegraf.mkd
* InfluxDB -> InfluxDB.mkd
* Grafana -> Grafana.mkd

in the document folder "doc". Start with "MQTT-Telegraf-InfluxDB-Grafana.mkd".

## References

InfluxDB Line Protocol Tutorial [https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial] last accessed Jan 2020
