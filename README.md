# Decoder for Bresser 3CH 433MHz Temperature/Humidity Sensor 

This repository shows how to build a receiver/decoder for an 433 MHz Bresser 3 channel temperature and humidity sensor.
The only components are an Arduino (Nano recommended) and a RXB6 433 MHz receiver.

An Arduino sketch decodes the RF signals and sends the decoded data as JSON string to the USB-Serial interface.
The intention is to use the decoded data on a smarthome system like ioBroker.

A script for integration ioBroker is also available in this repository.

