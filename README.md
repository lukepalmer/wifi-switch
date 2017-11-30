# WiFi Battery Switch

A Wi-Fi enabled switch that remotely controls a load connected to a 12V battery, monitors the battery, and draws very little power at idle.

## Problem

I want to, occasionally, remotely power up a large 1.5kW power inverter that's connected to a 12V battery and runs a block heater. The battery is large and is charged slowly by a by a solar panel. I also want to remotely see the level of charge of the battery.

Some solutions exist for this (search ebay for 12V WiFi relay). I tried a few but unfortunately found that:
* They waste lots of power at idle, which is no good for solar
* Control software was tempermental and unreliable
* They don't monitor the battery

So... I made one that doesn't suck.

## Outline

The device is based on an ESP8266 running Arduino with [Blynk](http://www.blynk.cc). It drives a high-side MOSFET switch to control a load and uses the onboard ADC to monitor the battery. It talks via WiFi to Blynk, which in turn uses an Android or iPhone app to switch the load on/off and display battery voltage.

The device should cost under $10 to build.

## Circuitry

### Parts

The circuit is very simple and consists of:

* Wemos D1 Mini (Based on the ESP8266) ($3, ebay)
* 3.3V 0.6A Step Down Buck Power Supply ($1, ebay)
* NPN switching transistor, 2N2222 or similar
* P-channel power MOSFET, IRF9540 or similar
* A few resistors

### Schematic

![Schematic](https://github.com/lukepalmer/wifi-switch/blob/master/schematic.png)

Brief description:
* A small switching power supply with very low quiescent current is used to provide 3.3V directly to the D1 mini. The linear regulator on the D1 mini is bypassed.
* The D1 mini's ADC is used to monitor battery voltage via a voltage divider
* The D1 mini controls a high-side power MOSFET that can switch a 12V load. This can handle 10A with no problem, probably 20A with a heatsink.

Note: The voltage divider must be connected directly to the ADC on the ESP8266 chip (A0) instead of the A0 pin on the D1 mini. This is because the D1 mini has an onboard voltage divider that is set up for 3.3v operation but we need to work with a full range of closer to 15V.

### Build

Here's what it looks like stuffed onto a protoboard. Nice and ugly.

![Top](https://github.com/lukepalmer/wifi-switch/blob/master/top.jpg)
![Bottom](https://github.com/lukepalmer/wifi-switch/blob/master/bottom.jpg)

## Software

### Firmware

### Blynk App