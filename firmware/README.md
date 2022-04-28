# blinky_ota について

blinky_otaは，ESP32用ファームウェアです．
ESP32デバイスの制御のためのAPIに加え，OTA (Over The Air) でプログラムの更新を行う機能を有しています。

# 準備
blinky_otaをESP32デバイスにインストールするためには，PC上にESP32の開発環境であるESP-IDF **v4.2**が必要です．
[ESP-IDF v4.2のインストール方法](https://docs.espressif.com/projects/esp-idf/en/v4.2.3/esp32/get-started/index.html)
に従い環境を構築してください．
（他のバージョンでは動作しません）．
Windows環境の場合は，**WSL 1**を利用して，Linux版の手順に従うことをおすすめします．

# ESP32デバイスへのBlinky-OTAのインストール
1. [このGitリポジトリ](https://github.com/onolab-tmu/blinky/tree/esp_v4.2)をクローン
   ```
   git clone -b esp_v4.2 https://github.com/onolab-tmu/blinky.git
   ```
3. ESP-IDF環境をexport
   ```
   get_idf
   ```
1. クローンしたリポジトリに移動
   ```
   cd path-to-dir/blinky/firmware/blinky_ota
   ```
1. コンパイル
   ```
   idf.py build
   ```
1. ESP32ボードに焼く
   ```
   idf.py -p /dev/ttyS* -b 115200 erase_flash flash
   ```

# OTAでのファームウェア更新
OTAでのファームウェア更新には、`idf.py menuconfig`での設定と、1度だけ有線接続での書き込みが必要となります。<br>
詳細は以下を参照ください。

---

## 1. menuconfig
blinky_otaには`idf.py menuconfig`で設定していただく項目があります。

```
cd blinky_ota
idf.py menuconfig
```
`Blinky Configuration`を選択してください。<br>
ここには、5つの設定項目があります。<br>
全Blinky共通の設定が4つ、個々のBlinky毎に設定する項目が1つあります。

### 1-1. 全Blinky共通の設定
まずは、全Blinkyで共通の設定項目が4つあります。

- #### WiFi SSID<br>

	使用する2.4GHz帯無線LANルータのSSIDを入力してください。

- #### WiFi Password<br>

	上記SSIDに対応するパスワードを入力してください。

- #### HTTP Server IP

	HTTP Server として使用する PC の IPアドレスを入力してください。

- #### HTTP Server Port

	HTTP Server で使用するポート番号を入力してください。<br>
	特に指定がなければデフォルト設定のままで問題ありません。


### 1-2. 個々のBlinkyでの設定
個々のBlinkyで設定する項目が1つあります。

- #### Individual number of blinky

	個体番号を入力してください。1以上の整数を想定しています。

## 2. 有線接続での書き込み
blinky_otaは OTA(Over The Air) に対応したプログラムですが、
最初に 有線で接続して書き込みを行う必要があります。

```
idf.py build
idf.py -p PORT -b 115200 erase_flash flash
```

ここで、`PORT`は`ls /dev/cu*`で出てきたESP32のポートを指定してください。

また、通常の書き込みと異なり、`erase_flash`が必要ですのでご注意ください。

## 3. 無線接続での書き込み
上記の設定、書き込みが完了していれば、次からはOTAでの書き込みが可能となります。

### 3-1. ターゲットの設定
書き込みを行うBlinkyを指定する為に、target.txtを編集します。<br>
target.txtはblinky_otaフォルダの直下に配置されています。<br>
target.txtを開くと以下のような構成になっています

```
add
1-50,60,70,80,90
remove
5-10,20,30
```

`add`の次の行が、書き込み対象となる個体番号の指定<br>
`remove`の次の行が、`add`の中から排除する個体番号の指定となります。<br>
個体番号の指定は `1-50`のようにすると1番から50番までを対象とします。<br>
また、`,`で区切ることでいくつでも数字を指定することができます。<br>

上記の例では実際に書き込み対象となる個体番号は

> 1から4, 11から19, 21から29, 31から50, 60, 70, 80, 90

となります。

また、target.txtが存在しない場合には、全部のBlinkyが書き込み対象となります。

### 3-2. ビルド
プログラムのビルドおよびハッシュ値の作成、<br>target.txtからターゲットとする個体番号の計算を行います。

```
idf.py app
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

#### 3-2-1. `idf.py app`

blinkyプログラムのビルドを行います。<br>
buildフォルダ直下にblinky_ota.binファイルが作成されます。<br>
このbinファイルが、実際に書き込まれるファイルとなります。

#### 3-2-2. `python hash.py`

build/blinky_ota.binファイルからハッシュ値を計算します。<br>
計算した値はbuildフォルダ直下にhash.txtとして保存されます。<br>
このハッシュ値は、書き込み時にハードウェアが持っているハッシュ値と比較し、同じ値であればbinファイルに更新がなかったと判断し、書き込みを行いません。

#### 3-2-3. `python constraint.py`

target.txtから、書き込み対象の個体番号を計算します。<br>
計算した値はbuildフォルダ直下にconstraint.binとして保存されます。<br>
書き込み時にこのデータを見て、ハードウェアの個体番号がこのデータに含まれていれば書き込みを行います。<br>
また、target.txtが無い場合には、buildフォルダのconstraint.binを削除し、全部のBlinkyを書き込み対象とします。

### 3-3. HTTP Server

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

### 3-4. Blinkyデバイスの起動
Blinkyデバイスの電源をオン、またはリセットすると以下の手順で書き込みを試みます。

- #### WiFi接続<br>
	WiFi接続に成功すると、赤色のLEDが点灯します。<br>
	WiFi接続できなかった場合は、Blinkyの動作に切り替わります。

- #### サーバー（PC）への接続<br>
	サーバーへの接続に成功すると、赤色と緑色のLEDが点灯します。<br>
	サーバーへ接続できなかった場合、Blinkyの動作に切り替わります。<br>
	（ここでサーバーに接続できない場合、接続失敗と判断するのに少し時間がかかります。）

- #### 個体番号のチェック<br>
	constraint.binから、ターゲットの個体番号リストを取得します。<br>
	取得した番号リスト内に自分の個体番号が含まれていない場合、Blinkyの動作に切り替わります。

- #### ハッシュ値の取得<br>
	ハッシュ値を取得し、Blinkyデバイスに保存されているハッシュ値と比較します。<br>もし、ハッシュ値が更新されていれば、デバイスのハッシュ値を更新します。<br>更新されていなかった場合、Blinkyの動作に切り替わります。

- #### 書き込み
	これまでのハッシュ値、個体番号のチェックを通過したのでデータの更新を試みます。<br>
	blinky_ota.binのダウンロード、書き込みを行います。<br>
	ダウンロード中はBlinkyデバイスのLEDが時計回りに点灯します。<br>
	時計回りにLEDが光っていれば、データの更新が行われていると判断してください。<br>

	無事書き込みが終了すると、プログラムが再起動します。<br>
	再起動後、ハッシュ値の比較が行われ、同じハッシュ値になっているのでBlinkyの動作に切り替わります。
