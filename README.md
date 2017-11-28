# WiFi Battery Switch

## Problem

I wanted to be able to control a load powered by a 12V battery via WiFi. It would also be nice to be able to monitor the condition of the battery remotely.

In my application a large 12V SLA battery is charged slowly by a solar panel. Occasionally I power up a large 1.5kW power inverter that's connected to a block heater. The inverter must be hardwired to the battery to handle this amount of power, but the internal inverter control circuitry can be switched with a reasonable amount of current (about 2A). 

Some solutions exist for this (search ebay for 12V WiFi relay). I tried a few but unfortunately found that:
* They waste lots of power at idle
* Control software was tempermental and unreliable

Let's make one that doesn't suck.

## Circuitry

### Parts

The circuit is very simple and consists of:

* Wemos D1 Mini (Based on the ESP8266) ($3, ebay)
* 3.3V 0.6A Step Down Buck Power Supply ($1, ebay)
* NPN switching transistor, 2N2222 or similar
* P-channel power MOSFET, IRF9540 or similar
* A few resistors

### Schematic

![Schematic](https://github.com/lukepalmer/wifi-switch/blob/master/schematic.png Schematic)

Brief description:
* A small switching power supply with very low quiescent current is used to provide 3.3V directly to the D1 mini. The linear regulator on the D1 mini is bypassed.
* The D1 mini's ADC is used to monitor battery voltage via a voltage divider
* The D1 mini controls a high-side power MOSFET that can switch a 12V load. This can handle 10A with no problem, probably 20A with a heatsink.

Note: The voltage divider must be connected directly to the ADC on the ESP8266 chip (A0) instead of the A0 pin on the D1 mini. This is because the D1 mini has an onboard voltage divider that is set up for 3.3v operation but we need to work with a full range of closer to 15V.

