Blinky
======

> **Blinky**
>
> Blinkies are small electronic devices that make very bright light (usually flashing) light using LEDs and small batteries.
>
> -- Wikipedia

The **Blinky** is a sound-to-light conversion device. When many blinkies are used together, it becomes possible to do large scale
audio array processing by acquiring the light from all devices simultaneously with a camera. We described the devices and a couple
of applications in a [paper](http://www.apsipa.org/proceedings/2018/pdfs/0001899.pdf). Applications are, for example

* Monitoring 
* Speech enhancement
* Sound source localization in very large and challenging environment
* Blind Source Separation [arxiv](https://arxiv.org/abs/1904.02334)

Hardware
--------

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


Firmware
--------

### Getting started

1. Install the ESP-IDF by following the official instructions
    ([stable](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html))
    ([latest](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)).
    In case it the first time working with the ESP32, it is recommended to try
    out a few example projects to learn how to compile and upload firmware to
    the device.

2. Build the firmware.

    ```
    cd firmware/blinky_ota

    # configure
    #  - serial port
    #  - device number
    #  - wireless network
    make menuconfig

    # compile and upload to device
    make flash
    ```

### Over-the-air Firmware upload

If the device was configured properly before compilation, it is possible
to do over-the-air (OTA) firmware updates.
The procedure is fairly simple.

1. Install a wireless router with the network SSID programmed in the Blinky
2. Start the server at `address`
3. Turn the Blinky off and on again

Then the Blinky should check the server for new firmware. If a new firmware is available, it will be downloaded and installed. The procedure will be accompanied by blinking of the LEDs.


### Wireless Microphone

The device can double as a wirelss microphone by changing the firmware.


Authors
-------

* The board was designed by [Evixar](https://www.evixar.com/) under directions from Robin Scheibler.
* The code was written by [Evixar](https://www.evixar.com/) and Robin Scheibler.

License
-------

MIT License, see `LICENSE.


