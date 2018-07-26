Blinky
======

> **Blinky**
>
> Blinkies are small electronic devices that make very bright light (usually flashing) light using LEDs and small batteries.
>
> -- Wikipedia

The **Blinky** is a sound-to-light conversion device. When many blinkies are used together, it becomes possible to do large scale
audio array processing by acquiring the light from all devices simultaneously with a camera. Applications are, for example

* Sound source localization in very large and challenging environment
* Monitoring 
* Speech enhancement

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


Authors
-------

* The board was designed by [Evixar](https://www.evixar.com/) under directions from Robin Scheibler.
* The code was written by [Evixar](https://www.evixar.com/) and by Robin Scheibler.

License
-------

MIT License, see `LICENSE`.


