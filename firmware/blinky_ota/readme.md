blinky_ota firmware について
===================

The `blinky_ota` firmware has two main functions

1. Blinky, i.e. sound-to-light conversion
2. Over-the-air (OTA) update, that is, the device will check a specific server on specific wifi network for firmware updates at boot time

The procedure to compile and flash the firmware the first time (via wire), and the subsequent
procedure for OTA updates are described.

---

Configuration of the firmware
-----------------------------

### 1. menuconfig

The initial configuration of the firmware is done by running `make menuconfig`

```
cd blinky_ota
make menuconfig
```

Choose `Blinky Configuration`.
There are 5 different options in this category.
Four of these are general to all the devices, but the last one is a device specific ID number.

### 1-1. General settings for all Blinkies

First, we configure the general settings.

- **WiFi SSID:** The SSID of the 2.4 GHz Wireless LAN that will be used for OTA.

- **WiFi Password:** The password of said network.

- **HTTP Server IP:** The IP address on this network of the HTTP server used for OTA.

- **HTTP Server Port:** The port on which the server is listening. If left blank, a default
  port will be used (8070).


### 1-2. Device-specific ID number configuration

The last setting is that of that device's ID number. This number will be used to target some firmware updates to only some devices.

- **Individual number of blinky:** The ID of the Blinky.


2. Flashing the Blinky via USB
------------------------------

The first time the `blinky_ota` firmware is uploaded to the device needs to be done via USB.

```
make erase_flash flash
```

Unlike most cases, for `blinky_ota`, we need to clear the flash memory first
via the `erase_flash` command.

3. Flashing the Blinky via Wifi (OTA)
-------------------------------------

Assuming all the above setup have been successfully carried out, it is now
possible to do OTA firmware update.

### 3-1. ターゲットの設定
### 3-1. Define the target devices

The update may be targeted to only some select devices by using the `server/target.txt` file.
Here is the content of an example target file.

```
add
1-50,60,70,80,90
remove
5-10,20,30
```

The command `add` by itself on a line indicates that the Blinkies listed by their
ID on the following line are candidates for firmware update.
The command `remove` indicates that on the contrary the ID listed on the following line
should not be updated.

The ID can be listed in comma separated list individually, e.g. `3,4,7`, in range, e.g. `1-5`, or a mix, e.g. `1, 3, 4-7`.

Note that `add` should come before `remove`, and they should both appear only once in the file.

In the above examples, the following ID are targeted:

```
1 to 4, 11 to 19, 21 to 29, 31 to 50, 60, 70, 80, 90
```

Finally, if the `target.txt` file does not exist, all Blinkies are targets for firmware update.
 
###3-2. ビルド
プログラムのビルドおよびハッシュ値の作成、<br>target.txtからターゲットとする個体番号の計算を行います。

```
make app
python hash.py
python constraint.py
```

上記の3つのコマンドをまとめた`build.sh`スクリプトも用意しています。
`build.sh`もblinky_otaフォルダ直下に配置していますので、macでしたら

```
./build.sh
```
を実行していただければ、3つのコマンドが実行されます。<br>
以下、3つのコマンドの簡単な解説となります。

####3-2-1. `make app`

blinkyプログラムのビルドを行います。<br>
buildフォルダ直下にblinky.binファイルが作成されます。<br>
このbinファイルが、実際に書き込まれるファイルとなります。

####3-2-2. `python hash.py`

build/blinky.binファイルからハッシュ値を計算します。<br>
計算した値はbuildフォルダ直下にhash.txtとして保存されます。<br>
このハッシュ値は、書き込み時にハードウェアが持っているハッシュ値と比較し、同じ値であればbinファイルに更新がなかったと判断し、書き込みを行いません。

####3-2-3. `python constraint.py`

target.txtから、書き込み対象の個体番号を計算します。<br>
計算した値はbuildフォルダ直下にconstraint.binとして保存されます。<br>
書き込み時にこのデータを見て、ハードウェアの個体番号がこのデータに含まれていれば書き込みを行います。<br>
また、target.txtが無い場合には、buildフォルダのconstraint.binを削除し、全部のBlinkyを書き込み対象とします。

###3-3. HTTP Server

サーバーを起動し、Blinkyデバイスへ無線経由で書き込みを行えるようにします。<br>

pyhon 2系の場合

```
cd build
python -m SimpleHTTPServer 8070
```

python 3系の場合<br>

```
cd build
python -m http.server 8070
```

pcは、***1. menuconfig*** で設定したWiFiへ接続し、ポート番号も設定したHTTP Server Port を指定してください。

###3-4. Blinkyデバイスの起動
Blinkyデバイスの電源をオン、またはリセットすると以下の手順で書き込みを試みます。

- ####WiFi接続<br>
	WiFi接続に成功すると、赤色のLEDが点灯します。<br>
	WiFi接続できなかった場合は、Blinkyの動作に切り替わります。

- ####サーバー（PC）への接続<br>
	サーバーへの接続に成功すると、赤色と緑色のLEDが点灯します。<br>
	サーバーへ接続できなかった場合、Blinkyの動作に切り替わります。<br>
	（ここでサーバーに接続できない場合、接続失敗と判断するのに少し時間がかかります。）

- ####個体番号のチェック<br>
	constraint.binから、ターゲットの個体番号リストを取得します。<br>
	取得した番号リスト内に自分の個体番号が含まれていない場合、Blinkyの動作に切り替わります。
	
- ####ハッシュ値の取得<br>
	ハッシュ値を取得し、Blinkyデバイスに保存されているハッシュ値と比較します。<br>もし、ハッシュ値が更新されていれば、デバイスのハッシュ値を更新します。<br>更新されていなかった場合、Blinkyの動作に切り替わります。

- ####書き込み
	これまでのハッシュ値、個体番号のチェックを通過したのでデータの更新を試みます。<br>
	blinky.binのダウンロード、書き込みを行います。<br>
	ダウンロード中はBlinkyデバイスのLEDが時計回りに点灯します。<br>
	時計回りにLEDが光っていれば、データの更新が行われていると判断してください。<br>
	
	無事書き込みが終了すると、プログラムが再起動します。<br>
	再起動後、ハッシュ値の比較が行われ、同じハッシュ値になっているのでBlinkyの動作に切り替わります。