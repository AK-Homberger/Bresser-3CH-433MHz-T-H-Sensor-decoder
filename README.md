# Decoder for Bresser 3CH 433MHz Temperature/Humidity Sensor with ioBroker Integration

This repository shows how to build a receiver/decoder for an 433 MHz [Bresser 3 channel](http://www.bresser.de/Wetter/BRESSER-Thermo-Hygro-Sensor-3CH-passend-fuer-BRESSER-Thermo-Hygrometer.html) temperature and humidity sensor.

It is also shown how to integrate the decoder into the [ioBroker](https://www.iobroker.net/) smarthome platform with a small script.

The only components needed are an [Arduino](https://www.makershop.de/plattformen/arduino/nano-v3/) (Nano recommended) and a [RXB6](https://www.makershop.de/module/funk/rxb6-433mhz-superheterodyne/) 433 MHz receiver. And of course, one or more Bresser 3CH sensors.

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Bresser3CH.JPG)

An [Arduino sketch](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/433MHz-Temperatur-Bresser-Nano/433MHz-Temperatur-Bresser-Nano.ino) decodes the data received from the RXB6 and sends the decoded data as JSON string to the USB-Serial interface.

The intention is to use the decoded data on a smarthome system like ioBroker.

# Hardware

Only a few hardware components are needed for this project. An Arduino and an RXB6 receiver and a few wires. That's all.

For the Arduino, I recommend a small [Nano](https://docs.arduino.cc/hardware/nano). But every Arduino should work. Just make sure to connect the receiver data to an Arduino pin that supports interrupts.

The **Arduino Nano** [(Pinout)](https://linuxhint.com/wp-content/uploads/2022/05/image3-64.png) has to be connected to the **RXB6** [(Pinout)](https://www.makershop.de/download/rxb6-pinout.jpg) receiver with 5 Volt/GND and the data output from the receiver with pin 2 from the Nano. The receiver needs an antenna. A simple wire with 17,3 cm length is sufficient. That's all for the hardware.

The breadboard picture shows how to connect.

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Arduino.JPG)

# Software

The [Arduino sketch](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/433MHz-Temperatur-Bresser-Nano/433MHz-Temperatur-Bresser-Nano.ino) has to be uploaded with the Arduino IDE to the Nano. Depending on the Nano type/version, it might be necessary to select **Processor: "ATmega328P (Old Bootloader)"** in the IDE to make the upload work.

After restart, the Arduino will wait for datagrams from one or more Bresser 3CH sensors. The sensors will send a telegram about every minute. Received datagrams are decoded and JSON formatted data is written to USB-Serial. 

The format is like this: **{"id":63, "ch":2, "temp":18.8, "hum":62, "lowbatt":0}**

- **"id"** is a random byte number (0-255) which is changing after every power loss (e.g. battery change)
- **"ch"** is the channel number (1-3) which can be set with a small switch inside the sensor
- **"temp"** is the temperature in °C
- **"hum"** is the humidity in %
- **"lowbatt"** is the battery state indicator (0=OK, 1=Low battery voltage)

The id for a sensor is changing randomly after every battery change. To get the current id of a sensor, you can use the Serial Monitor in the Arduino IDE to get the information. You will need the id for configuring the ioBroker script.

You can also use more than 3 sensors in ioBroker by defining the individual "id"/"ch" combination for a sensor.

## Programming Details
The core function in the Arduino sketch is the interrupt function **rx433Handler()**.
This function is called for every status change of the data line of the RXB6 receiver which is following the [On-off keying](https://en.wikipedia.org/wiki/On%E2%80%93off_keying) of the sending sensor. 

Within the function, the duration of every "high" pulse is measured and compared with the timing of the Bresser 3CH.

```
   +----+  +----+  +--+    +--+      high
   |    |  |    |  |  |    |  |
   |    |  |    |  |  |    |  |
  -+    +--+    +--+  +----+  +----  low
   ^       ^       ^       ^       ^  clock cycle
   |   1   |   1   |   0   |   0   |  translates as
```
- A long pulse of 500 us followed by a 250 us gap is a 1 bit,
- a short pulse of 250 us followed by a 500 us gap is a 0 bit.
- There is a sync preamble of pulse, gap, 750 us each, repeated 4 times.

Every valid "sync pulse" or invalid detected timing will reset the received data to zero.

For each vaild 0/1 received bit, the bit is stored and counted. If all 40 bits (32 bits data an 8 bits checksum) are received without detected invalid timings, the checksum is checked, and if correct, the data will be written to a ring buffer.

Within **loop()** the availability of new data in the ring buffer is constantly checked. If new data is available, the data will be decoded with the function **getResults()**. The decoded values are formatted as JSON string and sent to the USB-Serial interface (once per second for each sensor).

# ioBroker Integration Script

The JSON formatted output can be easily read with smarthome platforms like [ioBroker](https://www.iobroker.net/) or Home Assistant. 

The following script will show how to read the JSON data and set state values in ioBroker. To use the script, the Javascript/Blockly adapter has to be installed in ioBroker. 

For the script, I'm assuming ioBroker runs on a Raspberry. In the script, the device name for the USB-Serial adapter has to be set. If it is the only adapter, then the name **"/dev/ttyACM0"** should be the right name. Otherwise you can find out the name with "dmesg" command on the Raspberry after connecting the Nano to the Raspberry via USB.

The script creates state objects in ioBroker. The current script supports two sensors. If you need less or more, just comment out or duplicate/change the code for the sensor.

For each sensor you have to define the channel and the id. The channel can be set with a small switch inside the sensor. The sensor id will change randomly after a battery change of the sensor. Use the Arduino Serial Monitor for getting the id initially or after battery change . Alternatively you can uncomment this line "// console.log(data);" in the parser function. Then all datagrams are shown in the ioBroker log.

**If you do changes** in the script, make sure to **restart the Javascript instance** in ioBroker. Otherwise you will get an error related to a blocked serial device and the script will not work.

Script to be copied into ioBroker Javascript editor:
```
// Create a serial port
const { SerialPort } = require('serialport');
const port = new SerialPort({
  path: '/dev/ttyACM0',   // Set the device name
  baudRate: 115200,
});

// Create line parser
const { ReadlineParser } = require('@serialport/parser-readline');
const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

// Create states if they do not exist yet
createState("javascript.0.Temperature", 0, 
  {name: "Temperature",  role: 'variable', type: 'number'});

createState("javascript.0.Humidity", 0, 
  {name: "Humidity",  role: 'variable', type: 'number'});

createState("javascript.0.BatteryState", 0, 
  {name: "Battery State",  role: 'variable', type: 'number'});

createState("javascript.0.C2_Temperature", 0, 
  {name: "C2_Temperature",  role: 'variable', type: 'number'});

createState("javascript.0.C2_Humidity", 0, 
  {name: "C2_Humidity",  role: 'variable', type: 'number'});

createState("javascript.0.C2_BatteryState", 0, 
  {name: "C2 Battery State",  role: 'variable', type: 'number'});


// Define parser function

parser.on('data', function(data){
  // console.log(data);
  
  try{
    var messageObject = JSON.parse(data);
  }
  catch (e) {
    return console.error(e);
  }
 
  const temp =  messageObject.temp;
  const hum =  messageObject.hum;
  const ch = messageObject.ch;
  const id = messageObject.id;
  const lowbatt = messageObject.lowbatt;
  
  // Adjust channel and id to your needs

  if(ch == 1 && id == 4) {
    setState("javascript.0.Temperature", temp, true);
    setState("javascript.0.Humidity", hum, true);
    setState("javascript.0.BatteryState", lowbatt, true);
  }
  
  if(ch == 2 && id == 63) {
    setState("javascript.0.C2_Temperature", temp, true);
    setState("javascript.0.C2_Humidity", hum, true);
    setState("javascript.0.C2_BatteryState", lowbatt, true);
  }
});

```
An example on how to use the data in ioBroker, together with a VIS visualisation, can be found [here](https://github.com/AK-Homberger/D1Mini-GasCounter), where the Bresser sensor is used to get the outside temperature/humidity.

# Bresser 3CH Data Format

The sensor sends 15 identical packages of 40 bits each ~60s. The bits are pulse duration coded with [On-off keying](https://en.wikipedia.org/wiki/On%E2%80%93off_keying).
Transmissions also include channel code, sync id, batt-low, and test/sync.

Each transmission is 40 bits long (i.e. 29 ms, 36 incl. preamble).
32 bits data and 8 bits Modulo-256 checksum.

Data is transmitted in pure binary values.
Temperature is given in Centi-Fahrenheit and offset by 900.  Burst length is ~36ms (41 pulses + 8 syncs) * 750us.

- Channel 1 has a period of 57 s
- Channel 2 has a period of 67 s
- Channel 3 has a period of 79 s

```
   +----+  +----+  +--+    +--+      high
   |    |  |    |  |  |    |  |
   |    |  |    |  |  |    |  |
  -+    +--+    +--+  +----+  +----  low
   ^       ^       ^       ^       ^  clock cycle
   |   1   |   1   |   0   |   0   |  translates as
```
- A long pulse of 500 us followed by a 250 us gap is a 1 bit,
- a short pulse of 250 us followed by a 500 us gap is a 0 bit.
- There is a sync preamble of pulse, gap, 750 us each, repeated 4 times.

The data is grouped in 5 bytes / 10 nibbles:

```
 1111 1100 | 0001 0110 | 0001 0000 | 0011 0111 | 0101 1001 0  65.1 F 55 %
 iiii iiii | bscc tttt | tttt tttt | hhhh hhhh | xxxx xxxx
```
Meaning of bits:
- i: 8 bit random id (changes on power-loss)
- b: Battery indicator (0=>OK, 1=>LOW)
- s: Test/Sync (0=>Normal, 1=>Test-Button pressed / Sync)
- c: Channel (MSB-first, valid channels are 1-3)
- t: Temperature (MSB-first, Big-endian)
     12 bit unsigned fahrenheit offset by 90 and scaled by 10
- h: Humidity (MSB-first) 8 bit relative humidity percentage
- x: Checksum (byte1 + byte2 + byte3 + byte4) % 256
     

Basic information for this project has been found here: https://forum.iobroker.net/topic/3478/l%C3%B6sung-dekodieren-t-h-sensor-wetterstation-hygrometer-von-bresser-angebot-bei-lidl-14-99-uvp-40-433mhz

A lot of corrections/improvements were necessary to get a working solution with ioBroker integration. 
But thanks for the useful information regarding the timing.
