# Decoder for Bresser 3CH 433MHz Temperature/Humidity Sensor 

This repository shows how to build a receiver/decoder for an 433 MHz [Bresser 3 channel](http://www.bresser.de/Wetter/BRESSER-Thermo-Hygro-Sensor-3CH-passend-fuer-BRESSER-Thermo-Hygrometer.html) temperature and humidity sensor.
The only components are an Arduino (Nano recommended) and a RXB6 433 MHz receiver. And of course one or more Bresser 3CH sensors.

![Arduino](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Bresser3CH.JPG)

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Bresser3CH.JPG)

An [Arduino sketch](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/433MHz-Temperatur-Bresser-Nano/433MHz-Temperatur-Bresser-Nano.ino) decodes the RF signals and sends the decoded data as JSON string to the USB-Serial interface.
The intention is to use the decoded data on a smarthome system like ioBroker.

A script for integration ioBroker is also available in this repository.

