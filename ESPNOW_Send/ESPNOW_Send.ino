/**************************************
  PCからシリアル通信で受信してESP-NOWで指定のマイコンにLEDテープの制御データを送信する。　　　
 **************************************
  デバイスはM5StickC
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
//#include <M5StickC.h>
#include <WiFi.h>
#include <esp_now.h>

const uint8_t ESPNOW_CHANNEL = 0;  //ESP-NOWのチャンネル番号受信側のチャンネルと合わせる
// ESPNOWピア情報パラメータ
esp_now_peer_info_t peerInfo;
esp_err_t result;
esp_now_peer_num_t peerNum;

// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message {
  uint8_t resetF;     // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯
  uint8_t brightness; // LEDの明るさ(20~最大255)
  uint16_t ledLen;    // 接続しているLEDの最大数。ledColorに格納したバイト数
  uint16_t LedStep;   // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  String ledColor;    // LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;
struct_message myData; // myDataというstruct_messageを作成

//PCから受信するときのバッファ
#define DATASIZE 300   // 一度に送る文字列のバイト数
char buffArr[DATASIZE];
uint16_t bufp = 0;

String strData = "";
String tempStrData[6] = {"\0"}; // 分割された文字列を格納する配列
char inChar = '\n';

int split(String data, char delimiter, String *dst) {
  int index = 0;
  int arraySize = (sizeof(data) / sizeof((data)[0]));
  int datalength = data.length();
  for (int i = 0; i < datalength; i++) {
    char tmp = data.charAt(i);
    if ( tmp == delimiter ) {
      index++;
      if ( index > (arraySize - 1)) return -1;
    }
    else dst[index] += tmp;
  }
  return (index + 1);
}


int dataSet(const String &strData) {
  //文字列分割とデータ格納
  // 分割数 = 分割処理(文字列, 区切り文字, 配列)
  int index = split(strData, ',', tempStrData);

  //  if (index == 6) {
  //    Serial.printf("Data String Error");
  //    return 1;
  //  }
  //  if (tempStrData[1].length() != 17) {
  //    Serial.println("MacAddress length Err");
  //    return 2;
  //  }

  //MACアドレスを配列に変換
  uint8_t PeerAddress[6];
  char CmacBuf[16];
  tempStrData[0].toCharArray(CmacBuf, 16);
  char temp[3];
  uint8_t i;
  for (i = 0; i < 6; i++) {
    temp[0] = CmacBuf[i * 3 + 0];
    temp[1] = CmacBuf[i * 3 + 1];
    temp[2] = 0x00;
    PeerAddress[i] = strtol(temp, NULL, 16);
    Serial.printf("PeerAddr[ % x] = % x\n", i, PeerAddress[i]);
  }

  Serial.printf("resetF : %d \n", tempStrData[1].toInt());
  Serial.printf("brightness : %d \n", tempStrData[2].toInt());
  Serial.printf("ledLen : %d \n", tempStrData[3].toInt());
  Serial.printf("LedStep : %d \n", tempStrData[4].toInt());
  Serial.print("ledColor :" ); Serial.println(tempStrData[5]);

  //ESP-NOWに登録するピア情報
  memcpy(peerInfo.peer_addr, PeerAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;  //チャンネル
  peerInfo.encrypt = 0;               //暗号化しない

  //ESP-NOWで送信するデータを構造体に格納
  myData.resetF = 0;
  myData.brightness = 0;
  myData.ledLen = 0;
  myData.LedStep = 0;
  myData.ledColor = "";

  myData.resetF = tempStrData[1].toInt();
  myData.brightness = tempStrData[2].toInt();
  myData.ledLen = tempStrData[3].toInt();
  myData.LedStep = tempStrData[4].toInt();
  myData.ledColor = tempStrData[5];

  return 0;
}

//エラー情報
int EspNowErrF(esp_err_t &flg) {
  switch (flg) {
    case ESP_OK:
      Serial.println("ok");
      return 0;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESP_ERR_ESPNOW_NOT_INIT");
      return 1;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("ESP_ERR_ESPNOW_ARG");
      return 2;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("ESP_ERR_ESPNOW_INTERNAL");
      return 3;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
      return 4;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("ESP_ERR_ESPNOW_NOT_FOUND");
      return 5;
    case ESP_ERR_ESPNOW_EXIST:
      Serial.println("ESP_ERR_ESPNOW_EXIST");
      return 6;
    default:
      Serial.println("Err");
      return 7;
  }
  return 0;
}


//ESP-NOWでデータ送信
int EspNowSend() {
  const uint8_t *peer_addrP = peerInfo.peer_addr;

  // ピアが存在しない場合
  if (esp_now_is_peer_exist(peerInfo.peer_addr) == false) {
    // ピアツーピアリストを追加
    result = esp_now_add_peer(&peerInfo);
    Serial.print(F("esp_now_add_peer :"));
    if ( EspNowErrF(result) > 0) {
      return 1;
    }
  }

  //ESP-NOWでデータ送信.引数はピアMACアドレス,送信するデータ,データの長さ
  result = esp_now_send(peer_addrP, (uint8_t *)&myData, sizeof(myData));
  Serial.print(F("esp_now_send :"));
  if ( EspNowErrF(result) > 0) {
    return 2;
  }

  // ピアが最大数に達している場合は削除
  Serial.printf("peerNum.total_num: %d /n", peerNum.total_num);
  Serial.printf("ESP_NOW_MAX_TOTAL_PEER_NUM: %d /n", ESP_NOW_MAX_TOTAL_PEER_NUM);
  if (peerNum.total_num >= ESP_NOW_MAX_TOTAL_PEER_NUM)
  {
    result = esp_now_del_peer(peer_addrP); //ピアリスト削除
    if ( EspNowErrF(result) > 0) {
      return 3;
    }
  }
  return 0;
}


// 送信コールバック データがMAC層で正常に受信された場合に返ってくる
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //Serial.print("Last Packet Sent to: ");
  Serial.print(macStr);
  //Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? ":Delivery Success" : ":Delivery Fail");
}

void InitESPNow() {
  WiFi.mode(WIFI_STA); // デバイスをWi-Fiステーションとして設定する
  WiFi.disconnect();
  delay(100);
  if (esp_now_init() == ESP_OK)
  {
    Serial.println(F("ESPNow Init Success"));
  }
  else
  {
    Serial.println(F("ESPNow Init Failed"));
    // ESP.restart();
  }
}


/*********************************************
              セットアップ関数
*********************************************/
void setup() {
  delay(200);
  M5.begin();
  strData.reserve(DATASIZE); // strDATAの文字列の格納領域をbyte数確保

  delay(50);
  //ESP-NOWの接続
  InitESPNow();

  // ESPNOWデータ送信のコールバック機能を登録
  esp_now_register_send_cb(OnDataSent);
  Serial.print(F("My MAC Address: "));
  Serial.println(WiFi.macAddress());
  Serial.println(F("Waiting for reception"));

  //LCD表示設定
  M5.Lcd.setBrightness(200);
  M5.Lcd.setTextSize(4);//文字の大きさを設定（1～7）
  M5.Lcd.setTextColor(RED); //文字色設定(背景は透明)(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.fillScreen(BLACK);
}


void loop() {
  M5.update();
  if ( M5.BtnA.wasPressed() )  //ボタンAを押したとき
  {
    //バッファクリア
    strData = "";
    bufp = 0;
    while (Serial1.available() > 0)
    {
      (char)Serial.read();
    }
    Serial.println(F("Buffer cler"));
  }
  else if ( M5.BtnB.wasPressed() ) //ボタンBを押したとき
  {

    //testデータ
    inChar = '\n';
    strData = "08:3A:F2:68:5C:E4,2,100,50,2,RRRRGGBNNWWWWGBRGBR" + inChar;

    // データESP-NOWで送信
    if (dataSet(strData) == 0) {
      Serial.println(F("dataSet ok"));
      // データを構造体に入れる
      if (EspNowSend() == 0) {
        Serial.println(F("EspNowSend ok"));
      }
    }
    //バッファクリア
    strData = "";
    bufp = 0;
    while (Serial1.available() > 0)
    {
      (char)Serial.read();
    }
    Serial.println(F("Buffer cler"));

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0); M5.Lcd.print("ESP-NOW Test\n");
  }
  else if ( M5.BtnC.wasPressed() ) //ボタンCを押したとき
  {
    //今の所何もしない
  }


  //PCからシリアルデータの読み込み
  while (Serial.available() > 0)       // 受信したデータバッファが1バイト以上存在する場合
  {
    inChar = (char)Serial.read(); // Serialからデータ読み込み

    if (bufp >= DATASIZE)
    {
      //バッファクリア
      strData = "";
      bufp = 0;
      while (Serial1.available() > 0)
      {
        (char)Serial.read();
      }
      Serial.println(F("Error data baffer over"));
    }

    // 改行コード(LF)がある場合の処理
    if (inChar == '\n')
    {
      buffArr[bufp] = '\0';

      strData += buffArr[bufp];

      Serial.println(strData);

      if (bufp >= 27)
      {
        // データESP-NOWで送信
        if (dataSet(strData) == 0) {
          Serial.println(F("dataSet ok"));
          // データを構造体に入れる
          if (EspNowSend() == 0) {
            Serial.println(F("EspNowSend ok"));
          }
        }
      } else {
        Serial.println("data NG");
      }
      //バッファクリア
      strData = "";
      bufp = 0;
      buffArr[bufp] = '\0';

      while (Serial1.available() > 0)
      {
        char inChar = (char)Serial.read();
      }
    }
    else if (inChar != '\r')   // CRの改行コードの場合は結合しない
    {
      buffArr[bufp] = inChar; // 読み込んだデータを結合
      strData += inChar;
      bufp ++;
    }
  }
  delay(1);
}
