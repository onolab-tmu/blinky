Microserver for OTA
===================

Setting up an access point and server can be cumbersome, especially when on the move or doing experiments in an unfamiliar location.
To make the process easier, we created a micro-server based on an ESP32 device with an SD card reader such as the [WEMOS D32 Pro](https://wiki.wemos.cc/products:d32:d32_pro).
The `microserver` folder contains a [Micropython](https://www.micropython.org) firmware that makes the device an access point for network with SSID `blinkynet` and password `iloveblinky`.
The firmware is to be compiled locally and transferred to the root of an SD card.
Then, when powered, the ESP32 device will look for `blinky.bin` in the SD card, compute its hash and create `hash.txt`, create `constraint.bin` if needed, and serve them on an HTTP server.

Configuration
-------------

The microserver is configured via the JSON file `microserver/config.json` containing the following default configuration.

    {
      "access_point" : {
        "config" : {
          "essid": "blinkynet",
          "password": "iloveblinky",
          "authmode": 3
        },
        "ifconfig" : ["192.168.0.2", "255.255.255.0", "192.168.0.2", "192.168.0.2"]
      },
      "server" : {
        "port" : 8070
      }
    }

This means the Blinkies should have the corresponding configuration

    CONFIG_WIFI_SSID="blinkynet"
    CONFIG_WIFI_PASSWORD="iloveblinky"
    CONFIG_SERVER_IP="192.168.0.2"
    CONFIG_SERVER_PORT="8070"

when compiled in Section 1 of `blinky_ota/README.md`.

Program the server device
-------------------------

Here we will program the ESP32 device that should be used for the microserver

    cd microserver

    # install the tools needed if necessary
    pip install esptool adafuit-ampy

    # flash the micropython firmware
    esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash
    esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 write_flash -z 0x1000 esp32-20190607-v1.11-37-g62f004ba4.bin

    # upload the microserver code
    ampy -p <serial_port> put config.json
    ampy -p <serial_port> put sdcard.py
    ampy -p <serial_port> put main.py

Update process
--------------

1. Compile and build the new firmware as described in Section 1 of `blinky_ota/README.md`.
2. Optionally, create a `target.txt` file as described in Section 3-1 of `blinky_ota/README.md`.
3. Copy `build/blinky.bin` (and `target.txt` if needed) to the root of an SD card.
4. Plug the SD card into the microserver device.
5. Power the microserver device.
6. Update Blinkies by power cycling them.


