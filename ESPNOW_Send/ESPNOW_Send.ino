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
  MACアドレス,マイコンのフラグ,利用するLEDの間隔,接続しているLEDの最大数,LEDごとの色フラグ\r\n
  "00:00:00:00:00:01,2,3,150,BNNBBRRRGGGBB\r\n"
*/

#include <M5StickC.h>
#include <WiFi.h>
#include <esp_now.h>

const uint8_t ESPNOW_CHANNEL = 0;  //ESP-NOWのチャンネル番号受信側のチャンネルと合わせる
// uint8_t PeerAddr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // 受信機のMACアドレスに書き換えます

esp_now_peer_info_t peerInfo;  // ESPNOWピア情報パラメータ
peerInfo.channel = ESPNOW_CHANNEL;
peerInfo.encrypt = false;

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

uint8_t PeerAddress[6]; //[0xFF,0xFF,0xFF,0xFF,0xFF,0xFF]
string strData = "";

//エラー情報
int EspNowErrF(const esp_err_t &flg) {
  switch (flg) {
    case ESP_OK:
      Serial.println("成功");
      return 0;
    case ESP_ERR_ESPNOW_NOT_INIT:
      Serial.println("ESPNOWは初期化されていません");
      return 1;
    case ESP_ERR_ESPNOW_ARG:
      Serial.println("引数が無効です");
      return 2;
    case ESP_ERR_ESPNOW_INTERNAL:
      Serial.println("内部エラー");
      return 3;
    case ESP_ERR_ESPNOW_NO_MEM:
      Serial.println("メモリ不足");
      return 4;
    case ESP_ERR_ESPNOW_NOT_FOUND:
      Serial.println("ピアが見つかりません");
      return 5;
    case ESP_ERR_ESPNOW_EXIST:
      Serial.println("ピアが存在しました");
      return 6;
    default:
      Serial.println("現在のWiFiインターフェースがピアのインターフェースと一致しません");
      return 7;
  }
  return 0;
}


//ESP-NOWでデータ送信
int EspNowSend(const uint8_t &PeerAddress[6] , const string &strData) {

  //文字列分割とデータ格納
  char *token; /* 分離後の文字列を指すポインタ */
  char *tp;
  tp = strtok(baffArr, ",");  //MACアドレス
  if (tp == NULL) { return; }

  tp = strtok(NULL, ",");     //リセットフラグ
  if (tp == NULL) { return; }
  myData.resetF = strtol(tp);

  tp = strtok(NULL, ",");     //LEDステップ数
  if (tp == NULL) { return; }
  myData.LedStep = strtol(tp);

  tp = strtok(NULL, ",");     //LEDの長さ
  if (tp == NULL) { return; }
  myData.ledLen = strtol(tp);

  tp = strtok(NULL, ",");     //LEDの色制御
  if (tp == NULL) { return; }
  memcpy(myData.ledColor, tp, sizeof(tp));


  // ピアが存在しない場合
  if (esp_now_is_peer_exist(PeerAddress) == false) {
    // ピアツーピアリストを追加
    memcpy(peerInfo.peer_addr, PeerAddress, 6);
    result = esp_now_add_peer(&peerInfo);
    if (EspNowErrorF(result) > 0) 
    {
      return;
    }
  }

  //ESP-NOWでデータ送信.引数はピアMACアドレス,送信するデータ,データの長さ
  result = esp_now_send(PeerAddr, (uint8_t *)&myData, sizeof(myData));
  if (EspNowErrorF(result) > 0)
  {
    //何もしない
  }

  // ピアが最大数に達している場合は削除
  selial.println(peerNum.total_num)
  if (peerNum.total_num = > ESP_NOW_MAX_TOTAL_PEER_NUM)
  {
    //ピアリスト削除
    result = esp_now_del_peer(&peerInfo);
    if (EspNowErrorF(result) > 0)
    {
        //何もしない
    }
  }
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
  return 0;
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
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error initializing ESP-NOW"));
    return;
  }

  // ESPNOWデータ送信のコールバック機能を登録
  esp_now_register_send_cb(OnDataSent);
  Serial.print(F("My MAC Address: "));
  Serial.println(WiFi.macAddress());
  Serial.println(F("Waiting for reception"));
}


void loop() {
  M5.update();
  if ( M5.BtnA.wasPressed() )  //ボタンAを押したとき
  {
    //バッファクリア
    strData = "";
    buffp = 0;
    while (Serial1.available() > 0)
    {
      (char)Serial.read();
    }
  }

  //PCからシリアルデータの読み込み
  while (Serial.available() > 0)       // 受信したデータバッファが1バイト以上存在する場合
  {
    char inChar = (char)Serial.read(); // Serialからデータ読み込み
    
    // 改行コード(LF)がある場合の処理
    if (inChar == '\n')
    {
      buffArr[bufp] = '\0';
      if(buffArr.length() =>25)
      {
        //MACアドレス以降の受信した文字列のデータを結合
        for (int i = 18; i <= bufp; i++)
          strData += buffArr[bufp]
        }
        // データESP-NOWで送信
        if(EspNowSend(PeerAddress , strData) == 1 ){
          Serial.println(F("データフォーマット不正。"));
        }
      }
      //バッファクリア
      strData = "";
      buffp = 0;
      while (Serial1.available() > 0)
      {
        (char)Serial.read();
      }
    }
    else if (inChar != '\r')    // CRの改行コードの場合は結合しない
    {
      buffArr[buffp] = inChar; // 読み込んだデータを結合
      bufp ++;
      if(buffp == 17) //MACアドレスを配列に変換
      {
        char temp[3];
        uint8_t i;
        for (i = 0; i < 6; i++) {
          temp[0] = buffArr[i * 3 + 0];
          temp[1] = buffArr[i * 3 + 1];
          temp[2] = 0x00;
          PeerAddress[i] = strtol(temp, NULL, 16);
          Serial.printf("PeerAddr[%d] = %d\n", i, PeerAddress[i]);
        }
        Serial.println("");
      }
      else if (buffp >= DATASIZE)
      {
        //バッファクリア
        strData = "";
        buffp = 0;
        while (Serial1.available() > 0)
        {
          (char)Serial.read();
        }
        selial.println(F("データフォーマット不正。バッファを超えています。"));
      }
    }
  }
  delay(1);
}