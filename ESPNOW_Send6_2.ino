/**************************************
  PCからシリアル通信で受信してESP-NOWで指定のマイコンにLEDテープの制御データを送信する。
 **************************************
  デバイスは:M5Stack Basic (M5Stack-Core-ESP32)
 
  ESP-NOWはWPA2標準で使用するための標準暗号化プロトコル
  ESP32の開発会社であるEspressifが開発した通信方式で、
  内容的にはIEEE 802.11のCCM Protocolを使って通信
  esp_now_add_peer() 送信先登録：登録最大数は20台
   ※TODO 21台以上は送信先を削除して対応できるコード記述したが、私のお金がないため未検証。
  
  esp_now_send()     送信：送信先MACアドレスを指定して、250バイトまでのデータを送信可能
  USBのボーレート UART0(USB):115200bps
  デフォルトは8bit、パリティなし、1ストップビット
  PCからの送信データイメージ
  MACアドレス,マイコンのフラグ,LEDの明るさ,接続しているLEDの最大数,利用するLEDの間隔,LEDごとの色フラグ\r\n
  //PCの送信データ例"C0:49:EF:69:F9:04,2,100,50,2,RRRRGGBNNWWWWGBRGBR\n"
  LEDを制御できる最大数：200個(通信するための構造体struct_messageのledColorの数-1) 
*/

#include <M5Stack.h>
#include <WiFi.h>
#include <esp_now.h>

#define SOFTWERE_VER 6.20  // 画面に表示するソフトウェアバージョン

//ESP-NOWで送信するデータを構造体に格納
// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message {
  uint8_t resetF;      // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯4=Resive側のLED Testプログラムを動かす
  uint8_t brightness;  // LEDの明るさ(20~最大255)
  uint16_t ledLen;     // 接続しているLEDの最大数。ledColorに格納したバイト数
  uint16_t LedStep;    // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  char ledColor[201];  // LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;
struct_message myData;  // myDataというstruct_messageを作成

const uint8_t ESPNOW_CHANNEL = 0;  // ESP-NOWのチャンネル番号受信側のチャンネルと合わせる
esp_now_peer_info_t peerInfo;      // ESPNOWピア情報パラメータ


//PCから受信するときのバッファ
#define MAX_BUFFER_DATASIZE 250  // 一度に送る文字列のバイト数
char buffArr[MAX_BUFFER_DATASIZE];
uint16_t bufp = 0;

char CHAR_NULL = '\0';
char CHAR_CR = '\r';
char CHAR_LF = '\n';


//String文字列をString配列にカンマごとに分割
uint8_t splitStr(const String sData, const char delimiter, String *dst) {
  uint8_t splitCnt = 0;
  uint8_t arraySize = (sizeof(sData) / sizeof((sData)[0]));
  uint8_t datalength = sData.length();

  for (uint8_t i = 0; i < datalength; i++) {
    char tmp = sData.charAt(i);

    if (tmp == delimiter) {
      splitCnt++;
      if (splitCnt > (arraySize - 1)) return -1;
    } else dst[splitCnt] += tmp;
  }

  return (splitCnt + 1);  // 分割数
}

// 文字列を分割して構造体変数に格納する。
uint8_t ESPNOWsetData(const String &str) {

  // 文字列分割とデータ格納
  String tempStrData[6] = { "\0" };  // 分割された文字列を格納する配列
  splitStr(str, ',', tempStrData);   // 分割数 = 分割処理(文字列, 区切り文字, 配列)

  // Serial.println("split Data");
  // Serial.println(tempStrData[0]);
  // Serial.println(tempStrData[1]);
  // Serial.println(tempStrData[2]);
  // Serial.println(tempStrData[3]);
  // Serial.println(tempStrData[4]);
  // Serial.println(tempStrData[5]);

  // MACアドレスを配列に変換
  uint8_t PeerAddress[6];
  char CmacBuf[18];
  tempStrData[0].toCharArray(CmacBuf, 18);
  char temp[3];

  for (uint8_t i = 0; i < 6; i++) {
    temp[0] = CmacBuf[i * 3 + 0];
    temp[1] = CmacBuf[i * 3 + 1];
    temp[2] = 0x00;
    PeerAddress[i] = strtol(temp, NULL, 16);
    // Serial.printf("PeerAddr[ % x] = % x\n", i, PeerAddress[i]);
  }

  // ESP-NOWに登録するピア情報
  memcpy(peerInfo.peer_addr, PeerAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;  // チャンネル
  peerInfo.encrypt = 0;               // 暗号化しない

  // 構造体にデータ格納
  myData.resetF = tempStrData[1].toInt();
  myData.brightness = tempStrData[2].toInt();
  myData.ledLen = tempStrData[3].toInt();
  myData.LedStep = tempStrData[4].toInt();

  char ledColorBuf[tempStrData[5].length() + 1];
  tempStrData[5].toCharArray(ledColorBuf, tempStrData[5].length() + 1);

  memcpy(myData.ledColor, ledColorBuf, sizeof(ledColorBuf));
  myData.ledColor[tempStrData[5].length() + 2] = '\0';

  // Serial.printf("resetF : %d\n", myData.resetF);
  // Serial.printf("brightness : %d\n", myData.brightness);
  // Serial.printf("ledLen : %d\n", myData.ledLen);
  // Serial.printf("LedStep : %d\n", myData.LedStep);

  return 0;
}

bool ESPNOW_AddPeer() {
  // ESPNOW接続前にペアリストにMACアドレスを登録
  esp_err_t addStatus = esp_now_add_peer(&peerInfo);

  if (addStatus == ESP_OK) {
    // Pair success
    // Serial.println("Pair success");
    return true;
  } else if (addStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
    return false;
  } else if (addStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
    return false;
  } else if (addStatus == ESP_ERR_ESPNOW_FULL) {
    Serial.println("Peer list full");
    return false;
  } else if (addStatus == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("Out of memory");
    return false;
  } else if (addStatus == ESP_ERR_ESPNOW_EXIST) {
    Serial.println("Peer Exists");
    return true;
  } else {
    Serial.println("Not sure what happened");
    return false;
  }
}

void ESPNOW_deletePeer() {
  // ESPNOW　ペアリストを削除
  esp_err_t delStatus = esp_now_del_peer(peerInfo.peer_addr);

  Serial.print("Slave Delete Status: ");

  if (delStatus == ESP_OK) {
    // Delete success
    Serial.println("Success");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW Not Init");
  } else if (delStatus == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (delStatus == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

void ESPNOW_sendData() {
  // ESP-NOWでデータ送信.引数はピアMACアドレス,送信するデータ,データの長さ
  const uint8_t *peer_addrP = peerInfo.peer_addr;
  esp_err_t result = esp_now_send(peer_addrP, (uint8_t *)&myData, sizeof(myData));

  Serial.print("Send Status: ");

  if (result == ESP_OK) {
    // Serial.println("Success");
  } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
    // How did we get so far!!
    Serial.println("ESPNOW not Init.");
  } else if (result == ESP_ERR_ESPNOW_ARG) {
    Serial.println("Invalid Argument");
  } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
    Serial.println("Internal Error");
  } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
    Serial.println("Peer not found.");
  } else {
    Serial.println("Not sure what happened");
  }
}

//ESP-NOWでデータ送信
void EspNowSend() {

  // ピアが存在しない場合
  if (esp_now_is_peer_exist(peerInfo.peer_addr) == false) {
    // ピアツーピアリストを追加
    ESPNOW_AddPeer();
  }

  // ESP-NOWでデータ送信
  ESPNOW_sendData();

  ESPNOW_deletePeer();  //ピアリスト削除.関数呼び出し

  return;
}


// 送信コールバック データがMAC層で正常に受信された場合に返ってくる
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  Serial.println(macStr);
}



/*********************************************
              セットアップ関数
*********************************************/
void setup() {

  M5.begin(true, false, true, true);
  M5.Power.begin();

  // LCD表示設定
  //  M5.Lcd.sleep();
  //  M5.Axp.ScreenBreath(9); //明るさ：M5StickCの場合(0-12)
  M5.Lcd.setBrightness(30);  //明るさ：M5Stackの場合(0-255)
  //  M5.Lcd.setRotation(3); //画面の向き(0-3)
  M5.Lcd.setTextFont(2);       //フォントを指定(1-8)1=Adafruit 8ピクセルASCIIフォント,2=16ピクセルASCIIフォント
  M5.Lcd.setTextSize(3);       //文字の大きさを設定（1～7）
  M5.Lcd.setTextColor(WHITE);  //文字色設定(背景は透明)(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.fillScreen(BLACK);

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println(F("TanaPika Send"));

  M5.Lcd.setTextSize(2);  //文字の大きさを設定（1～7）

  M5.Lcd.print(F("Ver: "));
  M5.Lcd.println(SOFTWERE_VER);

  // UARTの通信設定を表示
  M5.Lcd.setTextSize(1);  //文字の大きさを設定（1～7）
  M5.Lcd.println("");
  M5.Lcd.println(F("UART Setting"));
  M5.Lcd.println(F("Baud Rate : 115200 bps"));
  M5.Lcd.println(F("Data Bit   : 8 bit"));
  M5.Lcd.println(F("Parity Bit : None"));
  M5.Lcd.println(F("Stop Bit   : 1 bit"));

  // ボタンの表示
  M5.Lcd.setTextColor(GREEN);  //文字色設定(背景は透明)(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.setTextSize(2);       //文字の大きさを設定（1～7）
  M5.Lcd.setCursor(30, 200);
  M5.Lcd.println(F("Reset   Clear   Test"));


  delay(50);

  //ESP-NOWの接続
  WiFi.mode(WIFI_STA);  // デバイスをWi-Fiステーションとして設定する
  WiFi.disconnect();
  delay(50);
  if (esp_now_init() == ESP_OK) {
    // Serial.println(F("ESPNow Init Success"));
  } else {
    Serial.println(F("ESPNow Init Failed"));
    delay(1000);
    ESP.restart();
  }

  // ESPNOWデータ送信のコールバック機能を登録
  esp_now_register_send_cb(OnDataSent);

  // バッファクリア 一応念のため。
  while (Serial.available() > 0) {
    Serial.read();
  }

  // Serial.println(F("Start"));
}


void loop() {

  M5.update();

  if (M5.BtnA.isPressed()) {
    // すべてのマイコンにリセット信号を送る。
    // String strAllReset = "C0:49:EF:69:F9:04,1,0,200,1,N\n";
    String strAllReset = "FF:FF:FF:FF:FF:FF,1,0,1,1,N\n";  // 全端末のリセット

    if (ESPNOWsetData(strAllReset) == 0) {  // データESP-NOWで送信するための構造体に格納
      EspNowSend();                         // 正常なデータであればデータ送信
    }

    delay(200);

    bufp = 0;
    buffArr[bufp] = CHAR_NULL;

    while (Serial.available() > 0) {
      Serial.read();
    }

  } else if (M5.BtnB.isPressed()) {
    // すべてのマイコンに消灯信号を送る。
    // String strAllClear = "C0:49:EF:69:F9:04,3,0,200,1,N\n";
    String strAllClear = "FF:FF:FF:FF:FF:FF,3,0,200,1,N\n";

    if (ESPNOWsetData(strAllClear) == 0) {

      EspNowSend();  // 正常なデータであればデータ送信
    }
    delay(200);

    bufp = 0;
    buffArr[bufp] = CHAR_NULL;

    while (Serial.available() > 0) {
      Serial.read();
    }

  } else if (M5.BtnC.isPressed()) {
    // すべてのマイコンにテスト用信号を送る。
    // String strAllClear = "C0:49:EF:69:F9:04,4,20,200,1,N\n";
    String strAllClear = "FF:FF:FF:FF:FF:FF,4,20,200,1,N\n";

    if (ESPNOWsetData(strAllClear) == 0) {

      EspNowSend();  // 正常なデータであればデータ送信
    }
    delay(4000);

    bufp = 0;
    buffArr[bufp] = CHAR_NULL;

    while (Serial.available() > 0) {
      Serial.read();
    }
  }

  // PCからシリアルデータの読み込み
  while (Serial.available() > 0)  // 受信したデータバッファが1バイト以上存在する場合
  {
    char inChar = (char)Serial.read();  // Serialから1バイト分データ読み込み

    // 不正入力などでバッファ溢れの時は再起動
    if (bufp > MAX_BUFFER_DATASIZE) {
      Serial.println(F("Buffer Over Error. ESP Restart"));
      delay(10);

      ESP.restart();
    }

    // 改行コード(LF)がある場合の処理
    if (inChar == CHAR_LF) {
      buffArr[bufp] = CHAR_NULL;

      if (bufp >= 25) {  //データのバイト数に不足がない場合に送信
        String strData = { "\0" };

        for (int i = 0; i < bufp; i++) {
          strData += (char)buffArr[i];
        }

        if (ESPNOWsetData(strData) == 0) {  // データESP-NOWで送信するための構造体に格納

          EspNowSend();  // 正常なデータであればデータ送信
          delay(2);
        }
      }

      bufp = 0;
      buffArr[bufp] = CHAR_NULL;

      while (Serial.available() > 0) {
        Serial.read();
      }

    } else if (inChar != CHAR_CR)  // 改行コード(CR)以外データを配列に格納。
    {

      buffArr[bufp] = inChar;  // 読み込んだデータを配列に格納
      bufp++;
    }
  }

  delay(1);
}