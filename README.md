Blinky Hardware
===============

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

The hardware is based on the ESP32 with a custom extension PCB bearing two MEMS
microphones and a few LEDs.
Details and necessary files are provided in the `hardware` folder.

Firmwares
---------

The folder `firmware` contains three different firmwares for the device.

### Blinky OTA Firmware

This is our working firmware that we have used for the device in our experiments.
It allows to do OTA updates and uses the DIP switch. This firmware reads sound
samples from the left microphone, compute the power for frames of 4 ms and apply
a non-linear transformation to map the value to the PWM range of the LED.
A few of the modes are for calibration only and just sweep the PWM duty cycles of the LED.

#### Quick Start

This section explains how to get the firmware running in a few commands.
For more detailed instructions, including over-the-air (OTA) updates, 
see `firmware/blinky_ota/README.md`.

1. Install the ESP-IDF by following the official instructions
    ([stable](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html))
    ([latest](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)).
    In case it the first time working with the ESP32, it is recommended to try
    out a few example projects to learn how to compile and upload firmware to
    the device.

2. Build the firmware.

        cd firmware/blinky_ota/firmware
        
        # configure
        #  - serial port
        #  - device number
        #  - wireless network
        make menuconfig
        
        # compile and upload to device
        make flash

### Recorder Firmware

This firmware turns the device into a wireless stereo microphone.
The instructions to use this firmware are in `firmware/recorder/BlinkyRecorder_REAMDE_jp.pdf` (japanese only for now).

### Blank Firmware

This firmware is a stripped down version of the regular Blinky firmware. Its role
is to highlight how to read samples from one or both microphones and control the LEDs.

Software
--------

The `python` folder contains the `blinkytools` python package. This package
offers a graphical user interface (GUI) to easily capture the signals from
multiple Blinkies using a video camera, or from a pre-recorded video.

The package can be install from [pypi](https://pypi.org/project/blinkytools/) directly.

    pip install blinkytools

Authors
-------

* The board was designed by [Evixar](https://www.evixar.com/) under directions from Robin Scheibler.
* The code was written by Robin Scheibler and [Evixar](https://www.evixar.com/).

License
-------

MIT License for all the software, see `LICENSE.

CC-BY-SA 4.0 for the hardware.
