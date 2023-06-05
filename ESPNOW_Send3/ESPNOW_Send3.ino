/**************************************
  PCからシリアル通信で受信してESP-NOWで指定のマイコンにLEDテープの制御データを送信する。　　　
 **************************************
  デバイスは、またはM5StackBasic
 
  ESP-NOWはWPA2標準で使用するための標準暗号化プロトコル
  ESP32の開発会社であるEspressifが開発した通信方式で、
  内容的にはIEEE 802.11のCCM Protocolを使って通信
  esp_now_add_peer() 送信先登録：登録最大数は20台
  esp_now_send()     送信：送信先MACアドレスを指定して、250バイトまでのデータを送信可能
  ボーレート UART0(USB):115200bps
  デフォルトは8bit、パリティなし、1ストップビット
  PCからの送信データイメージ
  MACアドレス,マイコンのフラグ,LEDの明るさ,接続しているLEDの最大数,利用するLEDの間隔,LEDごとの色フラグ\r\n
  //PCの送信データ例"08:3A:F2:68:5C:E4,2,100,50,2,RRRRGGBNNWWWWGBRGBR\r\n"
*/
#include <M5Stack.h>
#include <WiFi.h>
#include <esp_now.h>

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

const uint8_t ESPNOW_CHANNEL = 0;  //ESP-NOWのチャンネル番号受信側のチャンネルと合わせる
esp_now_peer_info_t peerInfo;      // ESPNOWピア情報パラメータ


//PCから受信するときのバッファ
#define DATASIZE 250  // 一度に送る文字列のバイト数
char buffArr[DATASIZE];
uint16_t bufp = 0;

bool sendF = true;  //MACアドレス層での通信が取れたときに送信できるようにするフラグ


//String文字列をString配列にカンマごとに分割
int split(const String data, const char delimiter, String *dst) {
  int index = 0;
  int arraySize = (sizeof(data) / sizeof((data)[0]));
  int datalength = data.length();
  for (int i = 0; i < datalength; i++) {
    char tmp = data.charAt(i);
    if (tmp == delimiter) {
      index++;
      if (index > (arraySize - 1)) return -1;
    } else dst[index] += tmp;
  }
  return (index + 1);
}

//文字列を分割して構造体に格納する。
int dataSet(const String &str) {
  sendF == false;

  //文字列分割とデータ格納
  String tempStrData[6] = { "\0" };          // 分割された文字列を格納する配列
  int index = split(str, ',', tempStrData);  // 分割数 = 分割処理(文字列, 区切り文字, 配列)

  //MACアドレスを配列に変換
  uint8_t PeerAddress[6];
  char CmacBuf[18];
  tempStrData[0].toCharArray(CmacBuf, 18);
  char temp[3];
  uint8_t i;
  for (i = 0; i < 6; i++) {
    temp[0] = CmacBuf[i * 3 + 0];
    temp[1] = CmacBuf[i * 3 + 1];
    temp[2] = 0x00;
    PeerAddress[i] = strtol(temp, NULL, 16);
    //    SerialBT.printf("PeerAddr[ % x] = % x\n", i, PeerAddress[i]);
  }

  //ESP-NOWに登録するピア情報
  memcpy(peerInfo.peer_addr, PeerAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;  //チャンネル
  peerInfo.encrypt = 0;               //暗号化しない

  //構造体にデータ格納
  myData.resetF = tempStrData[1].toInt();
  myData.brightness = tempStrData[2].toInt();
  myData.ledLen = tempStrData[3].toInt();
  myData.LedStep = tempStrData[4].toInt();

  char ledColorBuf[tempStrData[5].length() + 1];
  tempStrData[5].toCharArray(ledColorBuf, tempStrData[5].length() + 1);

  memcpy(myData.ledColor, ledColorBuf, sizeof(ledColorBuf));
  myData.ledColor[tempStrData[5].length() + 2] = '\0';
}

//エラー情報を表示 esp_err_t
int EspNowErrF(const esp_err_t &flg) {
  switch (flg) {
    case ESP_OK:
      return 0;
    case ESP_ERR_ESPNOW_NOT_INIT:
      SerialBT.print(F("ESP_ERR_ESPNOW_NOT_INIT"));  //ESPNOWは初期化されていません
      return 1;
    case ESP_ERR_ESPNOW_ARG:
      SerialBT.print(F("ESP_ERR_ESPNOW_ARG"));  //引数が無効です
      return 2;
    case ESP_ERR_ESPNOW_INTERNAL:
      SerialBT.print(F("ESP_ERR_ESPNOW_INTERNAL"));  //内部エラー
      return 3;
    case ESP_ERR_ESPNOW_NO_MEM:
      SerialBT.print(F("ESP_ERR_ESPNOW_NO_MEM"));  //メモリ不足
      return 4;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      SerialBT.print(F("ESP_ERR_ESPNOW_NOT_FOUND"));  //ピアが見つかりません
      return 5;
    case ESP_ERR_ESPNOW_EXIST:
      SerialBT.print(F("ESP_ERR_ESPNOW_EXIST"));  //ESPNOWピアが存在しました
      return 6;
    case ESP_ERR_ESPNOW_FULL:
      SerialBT.print(F("ESP_ERR_ESPNOW_FULL"));  //ピアリストがいっぱいです
      return 7;
    case ESP_ERR_ESPNOW_IF:
      SerialBT.print(F("ESP_ERR_ESPNOW_IF"));  //現在のWiFiインターフェースがピアのインターフェースと一致しません
      return 8;
    default:
      SerialBT.print(F("ESP_ERR"));
      return 9;
  }

  return 0;
}


//ESP-NOWでデータ送信
void EspNowSend() {
  esp_err_t result;
  esp_now_peer_num_t peerNum;

  const uint8_t *peer_addrP = peerInfo.peer_addr;

  // ピアが存在しない場合
  if (esp_now_is_peer_exist(peerInfo.peer_addr) == false) {
    // ピアツーピアリストを追加
    result = esp_now_add_peer(&peerInfo);

    // if (EspNowErrF(result) > 0) {
    //   SerialBT.println(F(": esp_now_add_peer"));
    //   return;
    // }
  }

  //ESP-NOWでデータ送信.引数はピアMACアドレス,送信するデータ,データの長さ
  result = esp_now_send(peer_addrP, (uint8_t *)&myData, sizeof(myData));

  // if (EspNowErrF(result) > 0) {
  //   SerialBT.println(F(": esp_now_send"));
  //   return;
  // }

  // ピアが最大数に達している場合は削除
  if (peerNum.total_num >= ESP_NOW_MAX_TOTAL_PEER_NUM) {
    result = esp_now_del_peer(peer_addrP);  //ピアリスト削除
    // if (EspNowErrF(result) > 0) {
    //   SerialBT.println(F(": esp_now_del_peer"));
    //   //return;
    // }
  }
  return;
}


// 送信コールバック データがMAC層で正常に受信された場合に返ってくる
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  // SerialBT.print(macStr);
  // SerialBT.println(status == ESP_NOW_SEND_SUCCESS ? ":OK" : ":NG");
  //  SerialBT.println(status == ESP_NOW_SEND_SUCCESS ? ":Delivery Success" : ":Delivery Fail");
  sendF == true;
  //SerialBT.println("Ready");
}


/*********************************************
              セットアップ関数
*********************************************/
void setup() {
  M5.begin();
  M5.Power.begin();

  Serial.bigin(115200);
  delay(50);

  //LCD表示設定
  //  M5.Lcd.sleep();
  //  M5.Axp.ScreenBreath(9); //明るさ：M5StickCの場合(0-12)
  M5.Lcd.setBrightness(180);  //明るさ：M5Stackの場合(0-255)
  //  M5.Lcd.setRotation(3); //画面の向き(0-3)
  M5.Lcd.setTextFont(2);       //フォントを指定(1-8)1=Adafruit 8ピクセルASCIIフォント,2=16ピクセルASCIIフォント
  M5.Lcd.setTextSize(3);       //文字の大きさを設定（1～7）
  M5.Lcd.setTextColor(WHITE);  //文字色設定(背景は透明)(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.fillScreen(BLACK);

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println(F("TanaPika Send"));
  M5.Lcd.println(F("Softwere Ver3.2"));

  delay(100);

  //ESP-NOWの接続
  WiFi.mode(WIFI_STA);  // デバイスをWi-Fiステーションとして設定する
  WiFi.disconnect();
  delay(50);

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("ESPNow Init Failed"));
    delay(500);
    ESP.restart();
  }
  Serial.println(F("ESPNow Init Success"));

  // ESPNOWデータ送信のコールバック機能を登録
  esp_now_register_send_cb(OnDataSent);

  sendF = true;
}


void loop() {

  //PCからシリアルデータの読み込み
  while (Serial.available() > 0)  // 受信したデータバッファが1バイト以上存在する場合
  {
    char inChar = (char)Serial.read();  // Serialからデータ読み込み

    if (bufp >= DATASIZE) {
      //バッファクリア
      bufp = 0;
      while (Serial.available() > 0) {
        Serial.read();
      }
    }

    // 改行コード(LF)がある場合の処理
    if (inChar == '\n') {
      buffArr[bufp] = '\0';

      String strData;
      strData.reserve(bufp);

      for (int i = 0; i < bufp; i++) {
        strData += (char)buffArr[i];
      }

      if (bufp >= 25 && sendF == True)  //データのバイト数に不足がない場合に送信
      {
        if (dataSet(strData) == 0) {  // データESP-NOWで送信するための構造体に格納
          EspNowSend();               // データ送信
          delay(1);
        }
      }

      //バッファクリア
      bufp = 0;
      buffArr[bufp] = '\0';
      while (Serial.available() > 0) {
        Serial.read();
      }
    } else if (inChar == '\r' || inChar == '\0')  // CRの改行コードの場合かNULLは結合しない
    {
      //なにもしない
    } else {
      buffArr[bufp] = inChar;  // 読み込んだデータを結合
      bufp++;
    }
  }

  delayMicroseconds(50);
}