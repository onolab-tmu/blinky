Hardware CAD Files
==================

This folder contains the hardware description, bill of material, and schematics necessary to build a Blinky.

Content
-------

* Kicad project files in the `kicad` folder.
* Illustration of the PCB from the bottom in `pcb.pdf`.
* Illustration of the PCB together with the location of holes in `pcb_and_holes.pdf`.
* CAD drawings of the enclosure and the holes drilled in it in `tw5-4-7_marking_top.pdf`.

The hardware is released under [CC-BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/) license.

Description
-----------

The hardware mainly consists of three components.

* ESP32 board such as the [Adafruit HUZZAH32](https://www.adafruit.com/product/3405)
    or [WEMOS D32](https://wiki.wemos.cc/products:d32:d32)
* MEMS digital microphone with I2S output (e.g. [SPH0645LM4H](https://www.adafruit.com/product/3421)
  or [ICS-43432](https://www.invensense.com/products/digital/ics-43432/))
* One (or more) LEDs (and current limiting resistors)

### Pin Mapping

Each device has a stereo microphone and four LEDs (red, green, blue, white). The microphones are connected
to I2S0 via these pins

    WS   <---> 14
    BCK  <---> 23
    DATA <---> 22

The LEDs are connected as follows

    WHITE <---> 15
    GREEN <---> 25 (DAC1)
    RED   <---> 26 (DAC2)
    BLUE  <---> 27

In addition, there is a DIP switch with three channels connected to

    DIP1 <---> 13
    DIP2 <---> 32
    DIP3 <---> 33

The switches connect to VCC and should thus be used with the pin configured as input with pull-down resistor.
They are intended to be change between a number of preset configurations.
