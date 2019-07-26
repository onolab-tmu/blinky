Blinky OTA Firmware
===================

The `blinky_ota` firmware has two main functions

1. Blinky, i.e. sound-to-light conversion
2. Over-the-air (OTA) update, that is, the device will check a specific server on specific wifi network for firmware updates at boot time

The procedure to compile and flash the firmware the first time (via wire), and the subsequent
procedure for OTA updates are described.

---

1 Configuration of the firmware
-------------------------------

Install the ESP-IDF by following the official instructions
([stable](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html))
([latest](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html)).
In case it the first time working with the ESP32, it is recommended to try
out a few example projects to learn how to compile and upload firmware to
the device.

The initial configuration of the firmware is done by running `make menuconfig`

    cd firmware
    make menuconfig

Choose `Blinky Configuration`.
There are 5 different options in this category.
Four of these are general to all the devices, but the last one is a device specific ID number.

#### General settings for all Blinkies

First, we configure the general settings.

- **WiFi SSID:** The SSID of the 2.4 GHz Wireless LAN that will be used for OTA.
- **WiFi Password:** The password of said network.
- **HTTP Server IP:** The IP address on this network of the HTTP server used for OTA.
- **HTTP Server Port:** The port on which the server is listening. If left blank, a default
  port will be used (8070).


#### Device-specific ID number configuration

The last setting is that of that device's ID number. This number will be used to target some firmware updates to only some devices.

- **Individual number of blinky:** The ID of the Blinky.


2 Flashing the Blinky via USB
-----------------------------

The first time the `blinky_ota` firmware is uploaded to the device needs to be done via USB.

    make erase_flash flash

Unlike most cases, for `blinky_ota`, we need to clear the flash memory first
via the `erase_flash` command.

3 Flashing the Blinky via Wifi (OTA)
------------------------------------

Assuming all the above setup have been successfully carried out, it is now
possible to do OTA firmware update.

### 3-1 Define the target devices

The update may be targeted to only some select devices by using the `server/target.txt` file.
Here is the content of an example target file.

    add
    1-50,60,70,80,90
    remove
    5-10,20,30

The command `add` by itself on a line indicates that the Blinkies listed by their
ID on the following line are candidates for firmware update.
The command `remove` indicates that on the contrary the ID listed on the following line
should not be updated.

The ID can be listed in comma separated list individually, e.g. `3,4,7`, in range, e.g. `1-5`, or a mix, e.g. `1, 3, 4-7`.

Note that `add` should come before `remove`, and they should both appear only once in the file.

In the above examples, the following ID are targeted:

    1 to 4, 11 to 19, 21 to 29, 31 to 50, 60, 70, 80, 90

Finally, if the `target.txt` file does not exist, all Blinkies are targets for firmware update.
 
### 3-2 Build

The three following commands will build the firmware, compute its hash, and create the constrint file containing the IDs of targeted devices.

    make app
    python hash.py
    python constraint.py

Alternatively, the script `build.sh` will run these three above commands.

    ./build.sh

We will now summarize what these three commands do.

#### 3-2-1 `make app`

This command compiles and builds the firmware.
The compiled firmware is the file `build/blinky.bin`.
This file is what actually gets written in the Flash memory of the device.

#### 3-2-2 `python hash.py`

This command computes the hash of the file `build/blinky.bin`.
The hash is saved as a string in the file `build/hash.txt`.
When doing over-the-air update, this hash value is compared to the one of the firmware currently saved in the device.
If the hash is different, the firmware is updated and the new hash saved to persistent memory.
If the hash is the same, the update is aborted.

#### 3-2-3 `python constraint.py`

This command will read the file `target.txt` and create a new binary file
`constraint.bin` containing the IDs of devices targeted by the update.  If
`target.txt` does not exist, then `constrain.bin` is not created.  When a
device connects to the server to be updated, it will first try to recover this
file.  If the file `constraint.bin` is not found, then the device will try to
update the firmware.  If the file `constraint.bin` is found, the device will
check whether it contains its own ID.  If it does, the update goes ahead.  If
it doesn't, the update is aborted.

### 3-3 HTTP Server

The HTTP server is created by running the following command assuming that Python 3 is installed

    cd build
    python -m http.server 8070

In Python 2, the command `python -SimpleHTTPServer 8070`.
The computer on which the command is run should be on the same network and with the IP defined
in [Section 1](#1-configuration-of-the-firmware). The port number should also be matching.

4 Microserver
-------------

Setting up an access point and server can be cumbersome, especially when on the move or doing experiments in an unfamiliar location.
To make the process easier, we created a micro-server based on an ESP32 device with an SD card reader such as the [WEMOS D32 Pro](https://wiki.wemos.cc/products:d32:d32_pro).
The `microserver` folder contains a [Micropython](https://www.micropython.org) firmware that makes the device an access point for network with SSID `blinkynet` and password `iloveblinky`.
The firmware is to be compiled locally and transferred to the root of an SD card.
Then, when powered, the ESP32 device will look for `blinky.bin` in the SD card, compute its hash and create `hash.txt`, create `constraint.bin` if needed, and serve them on an HTTP server.
Details of installation and use of the microserver are in `microserver/README.md`.

5 Boot process of a Blinky device
---------------------------------

When a Blinky is turned on or reset, the following things happen in order.

1. **Connection to the Wifi**
  
    If the connection to the Wifi network is successful, the red LED lights up.
    If the Wifi network is not found, the device goes into normal Blinky operation.

2. **Connection to the Server**

    If the connection to the server is successful, the red and green LEDs light up.
    If the connection is not successful, the device goes into normal Blinky operation.
    In case no server is running, the connection fails due to timeout which takes some time to happen.

3. #### Verification of device ID

    The Blinky attempts to retrieve the file `constraint.bin` from the server.
    If the file does not exist, the device proceeds to the next step.
    If the file exists, it is downloaded and its content checked.
    If it contains the device ID, the device proceeds to the next step.
    If it does not, the device goes into normal Blinky operation.

4. #### Verification of the firmware hash

    The Blinky retrieves the file `hash.txt` from the server.
    The persistent memory of the device is checked for the hash of the current firmware.
    If the memory does not contain a hash, the device goes proceeds to the next step.
    If the memory contains a hash different from that in `hash.txt`, the device proceeds to the next step.
    If the memory contains a hash and it is identical to that in `hash.txt`, the device goes into normal Blinky operations.

5. #### Firmware update

    The device retrieves the file `blinky.bin` from the server.
    During the download, the LEDs flash in a rotating pattern.
    Upon successful download of the file, its content is used to overwrite the current firmware of the device.
    After the firmware update, the device executes a hardware reset.
