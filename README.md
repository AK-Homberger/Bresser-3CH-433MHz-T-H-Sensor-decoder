# Decoder for Bresser 3CH 433MHz Temperature/Humidity Sensor 

This repository shows how to build a receiver/decoder for an 433 MHz [Bresser 3 channel](http://www.bresser.de/Wetter/BRESSER-Thermo-Hygro-Sensor-3CH-passend-fuer-BRESSER-Thermo-Hygrometer.html) temperature and humidity sensor.
The only components are an Arduino (Nano recommended) and a [RXB6](https://www.makershop.de/module/funk/rxb6-433mhz-superheterodyne/) 433 MHz receiver. And of course one or more Bresser 3CH sensors.

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Bresser3CH.JPG)

An [Arduino sketch](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/433MHz-Temperatur-Bresser-Nano/433MHz-Temperatur-Bresser-Nano.ino) decodes the RF signals and sends the decoded data as JSON string to the USB-Serial interface.
The intention is to use the decoded data on a smarthome system like ioBroker.

A script for integration ioBroker is also available in this repository.

# Hardware

Only a few hardware components are needed for this project. An Arduino an a RXB6 receiver and a few wires. That's all.

For the Arduino I reccomend a small Nano. But every Arduino should work. Just make sure to connect the receiver data to an Aduino pin that supports interrupts.

The Arduino has to be cennected to the RXB6 receiver with 5 Volt/GND and the data output from the receiver with pin 2 from the Nano. The receiver needs a antenna. A simple wire with 17,3 cm length is usually sufficient. That's all for the hardware.

![Bresser3CH](https://github.com/AK-Homberger/Bresser-3CH-433MHz-T-H-Sensor-decoder/blob/main/Arduino.JPG)

# Software

The Arduino sketch has to be uploaded with the Arduino IDE to the Nano.

After restart the Arduino will wait for datgrams from one or more Bresser 3CH sensors. The sensors will send a telegram about every minute. Receiver datagrams will be decoded and JSON formatted data will be written to USB-Serial. 

The format is like this: {"id":63, "ch":2, "temp":18.8, "hum":62, "lowbatt":0}

# ioBroker Integration Script
The JSON formatted output can be easily read with othe smarthome platforms like ioBroker or Home Assistant. The following script will show how to read the JSON data and set state values in ioBroker. To use the script, the JavaScript adapter has to be installed in ioBroker.

For the script I will assume that ioBroker runs on a Raspberry. In the script the device name for the USB-Serial adapter has to be set. If it is the only adapter the the name "/dev/ttyACM0" should be the right name.

The script will also create state object in ioBroker. The current script supports two sensors. If you need less ore more, just comment out or duplicate the code for the receiver.

For each receiver you have to define the channel and the id. The channel can be set with a small switch in the sensor. The sensor id will change randomly after a battery change in sensor. Use the Arduino Serial Monitor for getting the id initially or after battery change . Alternatively you can uncomment this line "// console.log(data);" in the parser function. Then all datagrams are shown in the ioBroker log.

If you do changes in the script, make sure to restart the Javascript instance in ioBroker. Otherwise you will get an error related to a blocked serial device.

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

createState("javascript.0.C2_Temperature", 0, 
  {name: "C2_Temperature",  role: 'variable', type: 'number'});

createState("javascript.0.C2_Humidity", 0, 
  {name: "C2_Humidity",  role: 'variable', type: 'number'});

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
  
  if(lowbatt == 1) {
        var msg = "Low Battery ID: "+ id + " Channel: " + ch;
        console.log(msg);
    }

  // Adjust channel and id to your needs

  if(ch == 1 && id == 4) {
    setState("javascript.0.Temperature", temp, true);
    setState("javascript.0.Humidity", hum, true);    
  }
  
  if(ch == 2 && id == 63) {
    setState("javascript.0.C2_Temperature", temp, true);
    setState("javascript.0.C2_Humidity", hum, true);
  }
});
```

# Bresser 3CH Data Format

The sensor sends 15 identical packages of 40 bits each ~60s. The bits are PWM modulated with On Off Keying.

Transmissions also include channel code, sync id, batt-low, and test/sync.

```
   +----+  +----+  +--+    +--+      high
   |    |  |    |  |  |    |  |
   |    |  |    |  |  |    |  |
  -+    +--+    +--+  +----+  +----  low
   ^       ^       ^       ^       ^  clock cycle
   |   1   |   1   |   0   |   0   |  translates as

```
- Each transmission is 40 bits long (i.e. 29 ms, 36 incl. preamble)
- 32 bits data and 8 bits CRC checksum.
- Data is transmitted in pure binary values, NOT BCD-coded.
- Temperature is given in Centi-Fahrenheit and offset by 900.  Burst length is ~36ms (41 pulses + 8 syncs) * 750us.

- CH1 has a period of 57 s
- CH2 has a period of 67 s
- CH3 has a period of 79 s

A short pulse of 250 us followed by a 500 us gap is a 0 bit,

a long pulse of 500 us followed by a 250 us gap is a 1 bit,

there is a sync preamble of pulse, gap, 750 us each, repeated 4 times.

The data is grouped in 5 bytes / 10 nibbles

```
 1111 1100 | 0001 0110 | 0001 0000 | 0011 0111 | 0101 1001 0  65.1 F 55 %

 iiii iiii | bscc tttt | tttt tttt | hhhh hhhh | xxxx xxxx
```
 

- i: 8 bit random id (changes on power-loss)

- b: battery indicator (0=>OK, 1=>LOW)

- s: Test/Sync (0=>Normal, 1=>Test-Button pressed / Sync)

- c: Channel (MSB-first, valid channels are 1-3)

- t: Temperature (MSB-first, Big-endian)

     12 bit unsigned fahrenheit offset by 90 and scaled by 10

- h: Humidity (MSB-first) 8 bit relative humidity percentage

- x: checksum (byte1 + byte2 + byte3 + byte4) % 256

     Check with e.g. (byte1 + byte2 + byte3 + byte4 - byte5) % 256) = 0

