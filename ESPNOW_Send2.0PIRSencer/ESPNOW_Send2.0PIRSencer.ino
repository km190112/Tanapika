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
  MACアドレス,マイコンのフラグ,LEDの明るさ,接続しているLEDの最大数,利用するLEDの間隔,LEDごとの色フラグ
  //PCの送信データ例"08:3A:F2:68:5C:E4,2,100,50,2,RRRRGGBNNWWWWGBRGBR"
*/
#include <M5StickC.h>
#include <WiFi.h>
#include <esp_now.h>

#define PIR_MOTION_SENSOR 36 //センサーピン
uint16_t SenserCount = 0;    //前回のセンサーの反応回数


//ESP-NOWで送信するデータを構造体に格納
// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message {
  uint8_t resetF;     // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯4=Resive側のLED Testプログラムを動かす
  uint8_t brightness; // LEDの明るさ(20~最大255)
  uint16_t ledLen;    // 接続しているLEDの最大数。ledColorに格納したバイト数
  uint16_t LedStep;   // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  char ledColor[201]; // LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;
struct_message myData; // myDataというstruct_messageを作成

const uint8_t ESPNOW_CHANNEL = 0; //ESP-NOWのチャンネル番号受信側のチャンネルと合わせる
esp_now_peer_info_t peerInfo;     // ESPNOWピア情報パラメータ


//String文字列をString配列にカンマごとに分割
int split(const String data, const char delimiter, String *dst) {
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

//文字列を分割して構造体に格納する。
int dataSet(const String &str) {

  Serial.print(F("strData : "));  Serial.println(str);

  //文字列分割とデータ格納
  String tempStrData[6] = {"\0"}; // 分割された文字列を格納する配列
  int index = split(str, ',', tempStrData);  // 分割数 = 分割処理(文字列, 区切り文字, 配列)

  // test 値確認
  //  Serial.print(F("PeerAddrLEN : "));  Serial.println(tempStrData[0].length());
  //  Serial.print(F("PeerAddr : "));  Serial.println(tempStrData[0]);
  //  Serial.printf("resetF : %d \n", tempStrData[1].toInt());
  //  Serial.printf("brightness : %d \n", tempStrData[2].toInt());
  //  Serial.printf("ledLen : %d \n", tempStrData[3].toInt());
  //  Serial.printf("LedStep : %d \n", tempStrData[4].toInt());
  //  Serial.print(F("ledColor : ")); Serial.println(tempStrData[5]);
  //  Serial.print(F("ledColor : ")); Serial.println(tempStrData[5].length());

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
    //    Serial.printf("PeerAddr[ % x] = % x\n", i, PeerAddress[i]);
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

  char ledColorBuf[tempStrData[5].length() + 1 ];
  tempStrData[5].toCharArray(ledColorBuf, tempStrData[5].length() + 1);

  memcpy(myData.ledColor, ledColorBuf, sizeof(ledColorBuf));
  myData.ledColor[tempStrData[5].length() + 2] = '\0';

  // test 値確認
  //  Serial.println("");
  //  Serial.printf("resetF : %d \n", myData.resetF);
  //  Serial.printf("brightness : %d \n", myData.brightness);
  //  Serial.printf("ledLen : %d \n", myData.ledLen);
  //  Serial.printf("LedStep : %d \n", myData.LedStep);
  //  Serial.printf("Ledsize : %d \n", sizeof(myData.ledColor));
  //
  //  Serial.print(F("ledColor : "));
  //  for (i = 0; i < sizeof(myData.ledColor); i++) {
  //    Serial.write(myData.ledColor[i]);
  //  }
  //
  //  Serial.println(F(""));

  //データ送信
  EspNowSend();
}

//エラー情報を表示 esp_err_t
int EspNowErrF(const esp_err_t &flg) {
  switch (flg) {
    case ESP_OK:
      Serial.println(F("ESP_OK"));
      return 0;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println(F("ESP_ERR_ESPNOW_NOT_INIT")); //ESPNOWは初期化されていません
      return 1;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println(F("ESP_ERR_ESPNOW_ARG")); //引数が無効です
      return 2;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println(F("ESP_ERR_ESPNOW_INTERNAL")); //内部エラー
      return 3;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println(F("ESP_ERR_ESPNOW_NO_MEM")); //メモリ不足
      return 4;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println(F("ESP_ERR_ESPNOW_NOT_FOUND")); //ピアが見つかりません
      return 5;
    case ESP_ERR_ESPNOW_EXIST:
      Serial.println(F("ESP_ERR_ESPNOW_EXIST")); //ESPNOWピアが存在しました
      return 6;
    case ESP_ERR_ESPNOW_FULL:
      Serial.println(F("ESP_ERR_ESPNOW_FULL")); //ピアリストがいっぱいです
      return 7;
    case ESP_ERR_ESPNOW_IF:
      Serial.println(F("ESP_ERR_ESPNOW_IF")); //現在のWiFiインターフェースがピアのインターフェースと一致しません
      return 7;
    default:
      Serial.println(F("ESP_ERR"));
      return 8;
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
    Serial.print(F("esp_now_add_peer :"));
    if ( EspNowErrF(result) > 0) {
      return;
    }
  }

  //ESP-NOWでデータ送信.引数はピアMACアドレス,送信するデータ,データの長さ
  result = esp_now_send(peer_addrP, (uint8_t *)&myData, sizeof(myData));
  Serial.print(F("esp_now_send :"));
  if ( EspNowErrF(result) > 0) {
    return;
  }

  // ピアが最大数に達している場合は削除
  //  Serial.printf("peerNum.total_num: %d \n", peerNum.total_num);
  //  Serial.printf("ESP_NOW_MAX_TOTAL_PEER_NUM: %d \n", ESP_NOW_MAX_TOTAL_PEER_NUM);
  if (peerNum.total_num >= ESP_NOW_MAX_TOTAL_PEER_NUM)
  {
    result = esp_now_del_peer(peer_addrP); //ピアリスト削除
    Serial.print(F("esp_now_del_peer :"));
    if ( EspNowErrF(result) > 0) {
      return;
    }
  }
  return;
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
  delay(50);
  if (esp_now_init() == ESP_OK)
  {
    Serial.println(F("ESPNow Init Success"));
  }
  else
  {
    Serial.println(F("ESPNow Init Failed"));
    delay(50);
    ESP.restart();
  }
}


/*********************************************
              セットアップ関数
*********************************************/
void setup() {
  M5.begin();

  pinMode(PIR_MOTION_SENSOR, INPUT);
  pinMode(GPIO_NUM_10, OUTPUT);    //内蔵LED
  digitalWrite(GPIO_NUM_10, HIGH); //内蔵LED消灯

  //ESP-NOWの接続
  InitESPNow();

  // ESPNOWデータ送信のコールバック機能を登録
  esp_now_register_send_cb(OnDataSent);

  Serial.print(F("My MAC Address: ")); Serial.println(WiFi.macAddress());

  //LCD表示設定
  M5.Axp.ScreenBreath(9);
  M5.Lcd.setRotation(1);     //画面の向きを指定(0～3)
  M5.Lcd.setTextFont(2);     //フォントを指定します。（1～8)1:Adafruit 8ピクセルASCIIフォント、2:16ピクセルASCIIフォント、4:26ピクセルASCIIフォント
  M5.Lcd.setTextSize(2);     //文字の大きさを設定（1～7）
  M5.Lcd.setTextColor(BLUE); //文字色設定(color,backgroundcolor)引数2番目省略時は(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.fillScreen(BLACK);  //画面全体を指定した色で塗りつぶし
  //  M5.Lcd.sleep();

  M5.Lcd.setCursor(0, 0); //画面の中のカーソルの位置を指定（ｘ，ｙ）
  M5.Lcd.println("PIR Sencer");
}

void loop() {
  String strData;

  M5.update();
  if ( M5.BtnA.wasPressed() ) //ボタンBを押したとき //リセット
  {
    Serial.println(F("Reset"));
    digitalWrite(GPIO_NUM_10, HIGH); //内蔵LED消灯
    SenserCount = 0;

    // データESP-NOWで送信
    strData = "4C:11:AE:EB:59:94,3,0,100,1,N";
    if (dataSet(strData) == 0) { }

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(GREEN);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println(F("LED Reset"));
  }

  int sensorValue = digitalRead(PIR_MOTION_SENSOR);
  Serial.print("sensorValue: "); Serial.println(sensorValue);

  if (sensorValue == LOW) //センサの反応がない時
  {
    if (SenserCount > 3)
    {
      Serial.print(F("Low"));
      digitalWrite(GPIO_NUM_10, HIGH); //内蔵LED消灯

      //LCD表示
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextColor(WHITE); //文字色設定
      M5.Lcd.println("PIR Sencer");
      M5.Lcd.println("Low");

      strData = "4C:11:AE:EB:59:94,3,0,200,1,N";
      if (dataSet(strData) == 0) { }

      SenserCount = 0;
    }

  }
  else { //センサが反応していた時
    if (SenserCount == 0)
    {
      digitalWrite(GPIO_NUM_10, LOW); //内蔵LED点灯
      Serial.println(F("High"));

      //LCD表示
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextColor(BLUE);
      M5.Lcd.println("PIR Sencer");
      M5.Lcd.println("High");

      // データESP-NOWで送信
      strData = "4C:11:AE:EB:59:94,2,150,100,1,RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRR";
      if (dataSet(strData) == 0) { }
    }
    SenserCount ++;
    //    delay(1000);
  }

  delay(500);
}
