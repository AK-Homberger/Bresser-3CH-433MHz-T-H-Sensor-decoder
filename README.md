# Decoder for Bresser 3CH 433MHz Temperature/Humidity Sensor 

This repository shows how to build a receiver/decoder for an 433 MHz [Bresser 3 channel](http://www.bresser.de/Wetter/BRESSER-Thermo-Hygro-Sensor-3CH-passend-fuer-BRESSER-Thermo-Hygrometer.html) temperature and humidity sensor.
The only components are an Arduino (Nano recommended) and a RXB6 433 MHz receiver. And of course one or more Bresser 3CH sensors.

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Bresser3CH.JPG)

An [Arduino sketch](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/433MHz-Temperatur-Bresser-Nano/433MHz-Temperatur-Bresser-Nano.ino) decodes the RF signals and sends the decoded data as JSON string to the USB-Serial interface.
The intention is to use the decoded data on a smarthome system like ioBroker.

A script for integration ioBroker is also available in this repository.

# Hardware

The Arduino, i reccomend a small Nano for thos project, has to cennected to the RXB6 receiver with 5 Volt/GND and the data output from the receiver with pin2 from the Nano. The receiver need a antenna. A simple wire with 17,3 cm is usually sufficient. Thats all for the hardware.

# Software

The Arduino sketch hast to be uploaded with the Arduino IDE to the Nano.

After restart the Arduino will wait for datgrams from one or more Bresser 3CH sensors. The sensors will send a telegram about every minute.

Receiver datagrams will be decoded and JSON formatted data will be written to USB-Serial. 

The format is like this: {"id":63, "ch":2, "temp":18.8, "hum":62, "lowbatt":0}


