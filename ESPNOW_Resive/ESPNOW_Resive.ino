/*
  ESP-NOW受信側のプログラム
  受信した信号からWS2812b RGBLEDを光らせる
  M5Stackを使用
  ESP-NOWはWPA2標準で使用するための標準暗号化プロトコル
  ESP32の開発会社であるEspressifが開発した通信方式で、内容的にはIEEE 802.11のCCM Protocolを使って通信
  esp_now_add_peer()  送信先登録：登録最大数は20台
  esp_now_send()      送信：送信先MACアドレスを指定して、250バイトまでのデータを送信可能
*/
#include <M5Stack.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>

//FastLED
#define LED_PIN     26 //LEDの信号出力のピン番号
// #define BRIGHTNESS  50 //LEDの明るさ
#define LED_TYPE    WS2812B
#define COLOR_ORDER RGB

// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message {
  int resetF;  // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯,4=LED動作テスト
  int LedStep; // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  int ledLen;   // 接続しているLEDの最大数。ledColorに格納したバイト数
  char [201] ledColor; // LEDごとに先頭から光らせる色を指定(R,G,B,Nのいずれかを指定)最大200番目まで設定可
} struct_message;

// myDataというstruct_messageを作成します
struct_message myData;

void InitESPNow() {
  WiFi.mode(WIFI_STA); // デバイスをWi-Fiステーションとして設定する
  WiFi.disconnect();
  delay(50);
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESPNow Init Success");
  }
  else
  {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }
}


// データを受信したときに実行されるコールバック関数
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  memcpy(&myData, incomingData, sizeof(myData));
  
  //シリアル出力
  Serial.print("MAC Address: ");  Serial.println(mac);
  Serial.print("Bytes received: ");  Serial.println(len);
  Serial.print("ResetFlag : "); Serial.println(myData.resetF);
  Serial.print("LedStep : "); Serial.println(myData.LedStep);
  Serial.print("ledLen : "); Serial.println(myData.ledLen);

  if(myData.resetF==1) //マイコンのリセット
  {
    FastLED.clear();
    FastLED.show();       //テープLEDに送信
    dylay(500)
    ESP.restart();
  }

  //FastLEDライブラリを利用してWS2812BのLEDテープを表示
  CRGB leds[myData.ledLen];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, myData.ledLen).setCorrection(TypicalLEDStrip);
  // FastLED.setBrightness(BRIGHTNESS);
  if(myData.resetF==2) // LEDをフラグにあわせて表示
  {
    for(int i=0;<=myData.ledLen;i++)
    {
      if(i > 0 && i % myData.LedStep != 0 && i => sizeof(myData.ledColor))
      {
        leds[i] = CRGB( 0, 0, 0);
      }
      else
      {
        switch (myData.ledColor[i])
        {
          case "R":
            leds[i] = CRGB( 255, 0, 0);
            break;
          case "G":
            leds[i] = CRGB( 0, 255, 0);
            break;
          case "B":
            leds[i] = CRGB( 0, 0, 255);
            break;
          case "N":
            leds[i] = CRGB( 0, 0, 0);
            break;
        }
      }
    }
    FastLED.show();       //テープLEDに送信
  }
  else if(myData.resetF>3) // LED全消灯
  {
    // for(int i=0;<=myData.ledLen;i++)
    // {
    //   leds[i] = CRGB( 0, 0, 0);
    // }
    FastLED.clear();
    FastLED.show();       //テープLEDに送信
  }
  else if(myData.resetF>4){ // LED動作テスト(順次LEDを光らせる)
    for(int i=0;<=myData.ledLen;i++)
    {
      leds[i] = CRGB( 200, 0, 0);
      FastLED.show();
      delay(200);
    }
    for(int i=0;<=myData.ledLen;i++)
    {
      leds[i] = CRGB( 0, 200, 0);
      FastLED.show();
      delay(200);
    }
    for(int i=0;<=myData.ledLen;i++)
    {
      leds[i] = CRGB( 0, 200, 0);
      FastLED.show();
      delay(200);
    }
  }
  // //M5StaLCD表示
  // M5.Lcd.fillScreen(BLACK);
  // M5.Lcd.setCursor(5, 5); M5.Lcd.println("Resive");
  // M5.Lcd.println(myData.resetF);
  // M5.Lcd.println(myData.LedStep);
  // M5.Lcd.println(myData.ledLen);
}


void setup()
{
  //引数(LCDを初期化,SDカードを使用するか, Serial通信をするか,I2C通信をするか)
  M5.begin(true, false, true, true);
  M5.Power.begin();
  M5.Lcd.setBrightness(200); //バックライトの輝度（0~255)
  M5.Lcd.fillScreen(BLACK);  //背景色(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.setTextColor(WHITE, BLACK); //文字色設定と背景色設定
  M5.Lcd.setTextSize(3);     //文字サイズ（1～7）
  M5.Lcd.setCursor(0, 0); M5.Lcd.println("Waiting for reception");

  Serial.println(WiFi.macAddress()); // このアドレスを送信側へ登録します
  delay(50);

  //ESP-NOWを初期化
  InitESPNow();

  //ESP-NOWを受信したら"OnDataRecv"を起動
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  delay(50);
  // M5.update();  // ボタン操作の状況を読み込む関数(ボタン操作を行う際は必須)
  
  // if (M5.BtnA.wasPressed()) // Aボタンが押された時:リセット
  // {
  //   Serial.println("Reset")
  //   esp_restart();
  // }
  // else if (M5.BtnB.wasPressed()) // Bボタンが押された時：画面暗くして省電力モード
  // {
  //   M5.Lcd.clear(); // 画面全体を消去
  //   M5.Lcd.setBrightness(0); //バックライトの輝度（0~255)
  // }
  // else if (M5.BtnC.wasPressed()) // Cボタンが押された時：MACアドレスを表示
  // {
  //   Serial.println(WiFi.macAddress()); // MACアドレスを出力
  //   M5.Lcd.clear(); // 画面全体を消去
  //   M5.Lcd.setBrightness(200); //バックライトの輝度（0~255)
  //   M5.Lcd.setCursor(0, 0); M5.Lcd.print(WiFi.macAddress());
  // }
}