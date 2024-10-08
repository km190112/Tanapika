# **１．概要**

部品棚にLEDテープ(WS2812b)を貼り付け、それと接続した受信機(ESP32)を送信機(M5StackBasic)で制御するシステム。

１つの受信機で最大200個のLEDを制御可能。

用途としては棚に保管したモノ探しにつかう。

開発環境はArduinoIDEでH/Wシステムの仕様と送信機と受信機の作り方公開する。


# **２．棚検索システムの機能**

(1)棚検索　　　：ほしい棚番の情報を取得し、対応するマイコンの指定したLED位置を光らせる。

(2)物品検索　　：ほしい物品管理番号の情報を取得し、対応するマイコンの指定したLED位置を光らせる。

(3)空き棚検索　：空いてる棚番号のリスト取得し、対応する対応するマイコンの指定したLED位置を光らせる。

(4)期限切れ検索：指定した日付未満有効期限の物品リストを取得し、対応する対応するマイコンの指定したLED位置を光らせる。


# **３．棚検索システムの構成**
PC

Database1.accdb(AccessDB)
       
tanaPikaV10.xlsm　ー　USBケーブルー送信機(M5StackBasic)　----　受信機(ESP32)　ー　LEDテープ()


# **４．H/W構成**

PCーUSBケーブルー送信機(M5StackBasic)----受信機(ESP32)ーLEDテープ()

※受信機はn台設置し制御可能。20台以上はまだ試したことがない。


# **５．使い方**

## (1)送信機(M5StackBasic)のボタン機能

　ボタンA:ブロードキャストですべての受信機のマイコンをリセット。

　ボタンB:ブロードキャストですべての受信機のマイコンのLEDを消灯。(使い終わったらLEDを消すため)

　ボタンC:ブロードキャストですべての受信機のマイコンのLED点灯テストを開始。(LEDの故障がないかチェック用)

## (2)送信機(M5StackBasic)の機能
 
 　点灯送信コマンドをUSB経由でPCからシリアルポートを接続して送信すると、コマンドに合わせて受信機が以下の振る舞いをする。
   
 　　①受信機のLED点灯、消灯、点灯テスト
    
 　　②受信機のマイコンリセット

　

# **６．利用者の初期設定**
PCと送信機を接続するために、PCにUSBシリアルドライバーをインストールする。

以下公式サイトからダウンロードし、PCにインストールする。
 
PCのOSによってインストールするドライバーが違うので注意。
 
https://docs.m5stack.com/en/download

M5StackBasic2.7のドライバ　→　CH9102F:CH9102_VCP_SER_Windows,CH9102_VCP_SER_MacOS v1.7,CH9102 MacOS(M1 Silicon),

M5StackBasic2.6以前のドライバ　→　CP2104 :CP210x_VCP_Windows,CP210x_VCP_MacOS,CP210x_VCP_Linux


# **７．受信機の制作**
・ESP32のGPIO26にLEDテープの信号線を接続する。

・電力供給はUSBでも可能だが、LEDの消費電力を考慮し、必要であれば直流安定化電源などからLEDとESP32への電力供給をする。
 
・大量の受信機とLEDを接続する場合直流安定化電源を購入したほうが安い場合がある。
 
・直流安定化電源と受信機の距離が離れる場合、電圧変動による誤動作が考えれられるため、
 
　必要であれば、ESP32の電源ーGND間に220μFのコンデンサを接続すると安定する。
 
・インストールするプログラム：ESPNOW_ResiveESP32ver3.ino
　※Arduino ESP32コアのESP32ボードアドオンバージョン2.X(ESP-IDF 4.4ベース)からバージョン3.0(ESP-IDF 5.1ベース)への変更されているが、最新のバージョンは現時点で動かない。
  
  インストールするボードマネージャ："ESP32 by Espressif Systens 2.0.17"
  
  インストールするライブラリ　　　："FastLED by Daniel Ver 3.7.1"
  
・FastLEDのライブラリを使用しています。


# **８．送信機の制作**

・インストールするプログラム：ESPNOW_Send6_2.ino
