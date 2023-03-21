/*
  This code is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This code is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

// Arduino (Nano) sketch to receive 433MHz telegrams from Bresser 3CH temperature/humidity sensor
// Use RXB6 receiver with 17,3 cm antenna and connect data pin with Arduino Nano pin 2 (plus 5V and GND)
// Generates JSON formatted data on USB-Serial: {"id":63, "ch":2, "temp":18.8, "hum":62, "lowbatt":0}
// Allows easy integration in other systems like ioBroker with a simple script.

// Version 1.0 - 20.03.2023

/*
  Bresser Thermo-/Hygro-Sensor 3CH
  Art. Nr.: 7009994
  EAN: 4007922031194

  http://www.bresser.de/Wetter/BRESSER-Thermo-Hygro-Sensor-3CH-passend-fuer-BRESSER-Thermo-Hygrometer.html
  https://forum.iobroker.net/topic/3478/l%C3%B6sung-dekodieren-t-h-sensor-wetterstation-hygrometer-von-bresser-angebot-bei-lidl-14-99-uvp-40-433mhz

  The sensor sends 15 identical packages of 40 bits each ~60s.
  The bits are PWM modulated with On Off Keying.
  Transmissions also include channel code, sync id, batt-low, and test/sync.

   +----+  +----+  +--+    +--+      high
   |    |  |    |  |  |    |  |
   |    |  |    |  |  |    |  |
  -+    +--+    +--+  +----+  +----  low
   ^       ^       ^       ^       ^  clock cycle

   |   1   |   1   |   0   |   0   |  translates as

  Each transmission is 40 bits long (i.e. 29 ms, 36 incl. preamble)
  32 bits data and 8 bits CRC checksum.
  Data is transmitted in pure binary values, NOT BCD-coded.
  Temperature is given in Centi-Fahrenheit and offset by 900.

  Burst length is ~36ms (41 pulses + 8 syncs) * 750us.
  CH1 has a period of 57 s
  CH2 has a period of 67 s
  CH3 has a period of 79 s

  A short pulse of 250 us followed by a 500 us gap is a 0 bit,
  a long pulse of 500 us followed by a 250 us gap is a 1 bit,
  there is a sync preamble of pulse, gap, 750 us each, repeated 4 times.

  Actual received and demodulated timings might be 2% shorter.
*/

#define RX433DATA 2          // Input pin for RXB6 receiver data (must be an interrupt supporting pin)

#define BRESSER3CH_SYNC 750  // Length in µs of SYNC pulse
#define BRESSER3CH_ONE 500   // Length in µs of ONE pulse
#define BRESSER3CH_ZERO 250  // Length in µs of ZERO pulse
#define BRESSER3CH_VAR 30    // Allowed pulse length variation for SYNC, ONE and ZERO pulses
#define BRESSER3CH_MESSAGELEN 40  // Number of bits in one message

#define FIFOSIZE 16  // Fifo Buffer size 16 can hold up to 15 items
volatile unsigned long fifoBuf[FIFOSIZE];     // Ring buffer
volatile byte fifoReadIndex, fifoWriteIndex;  // Read and write index into ring buffer


//*******************************************************************************
//
void setup() {
  Serial.begin(115200);
  while (!Serial);  // Wait for serial line to be ready

  pinMode(RX433DATA, INPUT);  // Define mode INPUT for receiver data pin

  // Enable interrupt handling for input pin related interrupt. 
  // Call interrupt routine "rx433Handler" for every line status change
  attachInterrupt(digitalPinToInterrupt(RX433DATA), rx433Handler, CHANGE);

  Serial.println("Start");
}


//*******************************************************************************
// Check if received modulo 256 checksum is correct for received data

boolean checksumValid(unsigned long data, byte checksum) {
  unsigned calculatedChecksum = 0;

  // Sum up received data bytes
  for (int i = 0; i < 4 ; i++) calculatedChecksum += ((data >> (i * 8)) & 0xFF);

  // Create calculated checksum by sum modulo 256
  calculatedChecksum %= 256;

  // Return true if claculated CRC equals received CRC
  return (calculatedChecksum == checksum);
}


//*******************************************************************************
// Write item into ring buffer

void fifoWrite(unsigned long item) {
  fifoBuf[fifoWriteIndex] = item; // Store the item
  if (!(fifoWriteIndex + 1 == fifoReadIndex || (fifoWriteIndex + 1 >= FIFOSIZE && fifoReadIndex == 0)))
    fifoWriteIndex++;  // Increase write pointer in ringbuffer
  if (fifoWriteIndex >= FIFOSIZE) fifoWriteIndex = 0; // Ring buffer is at its end
}


//*******************************************************************************
// Always check first if item is available with fifoAvailable()
// before reading the ring buffer using this function

unsigned long fifoRead() {
  unsigned long item;

  item = fifoBuf[fifoReadIndex];
  cli(); // Interrupts off while changing the read pointer for the ringbuffer
  fifoReadIndex++;
  if (fifoReadIndex >= FIFOSIZE) fifoReadIndex = 0;
  sei(); // Interrupts on again
  return (item);
}

//*******************************************************************************
// Item is available for reading if (fifoReadIndex!=fifoWriteIndex)

bool fifoAvailable() {
  return (fifoReadIndex != fifoWriteIndex);
}


//*******************************************************************************
// Interrupt function to receive data
// Called for every RF receiver line status change

void rx433Handler() {
  static unsigned long rx433LineUp = 0, rx433LineDown = 0;
  static unsigned long dataBits = 0;
  static byte checksumBits = 0;
  static unsigned bitsCounted = 0;

  unsigned long highPulseTime = 0;

  if (digitalRead(RX433DATA) == LOW) { // Receiver data line is LOW state

    rx433LineDown = micros(); // Line went DOWN after being HIGH before. Store time
    highPulseTime = rx433LineDown - rx433LineUp; // Calculate the HIGH pulse time

    if ((highPulseTime > BRESSER3CH_SYNC - BRESSER3CH_VAR) &&
        (highPulseTime < BRESSER3CH_SYNC + BRESSER3CH_VAR)) {

      // SYNC detected (750 µs pulse)
      dataBits = 0;
      checksumBits = 0;
      bitsCounted = 0;      
    }
    else if ((highPulseTime > BRESSER3CH_ONE - BRESSER3CH_VAR) &&
             (highPulseTime < BRESSER3CH_ONE + BRESSER3CH_VAR)) {

      // ONE bit detected (500 µs pulse)
      // Shift left, set bit and count up

      if (bitsCounted < 32) {   // Data bits (0 - 31)
        (dataBits <<= 1) |= 1;
      } else {                  // Checksum bits (32 - 39)
        (checksumBits <<= 1) |= 1;
      }
      bitsCounted++;      
    }
    else if ((highPulseTime > BRESSER3CH_ZERO - BRESSER3CH_VAR) &&
             (highPulseTime < BRESSER3CH_ZERO + BRESSER3CH_VAR)) {

      // ZERO bit detected (250 µs pulse)
      // Shift left and count up

      if (bitsCounted < 32) { // Data bits (0 - 31)
        dataBits <<= 1;
      } else {                // Checksum bits (32 - 39)
        checksumBits <<= 1;
      }
      bitsCounted++;      
    }
    else {  // Received bit is not a SYNC, ONE or ZERO bit, so restart
      dataBits = 0;
      checksumBits = 0;
      bitsCounted = 0;      
    }

    if (bitsCounted == BRESSER3CH_MESSAGELEN) { // All 40 bits received
      if (checksumValid(dataBits, checksumBits)) fifoWrite(dataBits); // Write valid value to FIFO buffer
      dataBits = 0;
      checksumBits = 0;
      bitsCounted = 0;      
    }
  }
  else {
    // HIGH state
    rx433LineUp = micros(); // Line went HIGH after being LOW before. Store time
  }
}


//*******************************************************************************
void getResults(unsigned long value, byte *Id, byte *Channel, float *Temp, byte *Hum, bool *lowBatt) {
  /*
      Meaning of 32 bits in value (value is without checksum!!!)

      0000 0100 | 0001 0110 | 0001 0000 | 0011 0111  = ID:4, CH:1, 65.1 F, 55 %
      iiii iiii | bscc tttt | tttt tttt | hhhh hhhh

      - i: 8 bit random id (changes on battery change)
      - b: battery indicator (0=>OK, 1=>LOW)
      - s: Test/Sync (0=>Normal, 1=>Test-Button pressed / Sync)
      - c: Channel (MSB-first, valid channels are 1-3)
      - t: Temperature (MSB-first, Big-endian) 12 bit unsigned fahrenheit offset by 90 and scaled by 10
      - h: Humidity (MSB-first) 8 bit relative humidity percentage
  */

  // ID
  *Id = (value >> 24) & 0xFF;

  // Battery state
  *lowBatt = (value >> 23) & 0x1;

  // Channel
  *Channel = (value >> 20) & 0x3;

  // Temperature
  unsigned int temperature = ((value >> 8) & 0xFFF);
  float fahrenheit = (temperature / 10.0) - 90.0;
  *Temp = (fahrenheit - 32.0) * (5.0 / 9.0);

  // Humidity
  *Hum = value & 0xFF;
}


//*******************************************************************************
void loop() {
  static unsigned long Timer = 0;
  static unsigned long old_dataReceived = 0;
  unsigned long dataReceived = 0;
  byte Id = 0;
  byte Channel = 0;
  float Temp = 0;
  byte Hum = 0;
  bool lowBatt = false;

  if (fifoAvailable() ) {
    dataReceived = fifoRead();

    getResults(dataReceived, &Id, &Channel, &Temp, &Hum, &lowBatt);

    // Send only one JSON message per second for one sensor
    if ((millis() - Timer > 1000) || dataReceived != old_dataReceived) {

      cli();  // Stop interrupts. Necessary for Nano to avoid damaged serial output for strange reasons!!!

      // Send JSON message
      Serial.print("{\"id\":");
      Serial.print(Id);
      Serial.print(", \"ch\":");
      Serial.print(Channel);
      Serial.print(", \"temp\":");
      Serial.print(Temp, 1);
      Serial.print(", \"hum\":");
      Serial.print(Hum);
      Serial.print(", \"lowbatt\":");
      Serial.print(lowBatt);
      Serial.print("}");
      Serial.println();

      sei(); // Allow interrupts again
      old_dataReceived = dataReceived;
      Timer = millis();
    }
  }
}
