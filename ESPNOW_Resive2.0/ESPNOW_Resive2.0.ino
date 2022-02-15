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
#define LED_PIN          26  //LEDの信号出力のピン番号
#define TEST_BRIGHTNESS  10  //テスト用 LEDの明るさ
#define TEST_LEDLEN     800 //テスト用 LEDの個数
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message
{
  uint8_t resetF;    // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯,4=LED動作テスト
  uint8_t brightness;// LEDの明るさ(20~最大255)
  uint16_t ledLen;   // 接続しているLEDの最大数。ledColorに格納したバイト数
  uint16_t LedStep;  // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  char ledColor[201];// LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;

// myDataというstruct_messageを作成します
struct_message myData;

bool busyF = false; //LED更新中に変更できないようにする。


void fastLedClear() {
  //FastLEDでLED全消灯
  CRGB leds[TEST_LEDLEN];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, TEST_LEDLEN).setCorrection(TypicalLEDStrip);
  FastLED.clear();
  FastLED.show();
  delay(50);
}


void LedTest() {
  fastLedClear();

  //FastLEDのテスト
  CRGB leds[TEST_LEDLEN];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, TEST_LEDLEN).setCorrection(TypicalLEDStrip);
  FastLED.clear();

  uint16_t i;

  for (i = 0; i < TEST_LEDLEN; i++)
  {
    leds[i] = CRGB( TEST_BRIGHTNESS, 0, 0);
    FastLED.show();
    delay(10);
  }

  FastLED.clear();
  FastLED.show();

  for (i = 0; i < TEST_LEDLEN; i++)
  {
    delay(10);
    leds[i] = CRGB( 0, TEST_BRIGHTNESS, 0);
    FastLED.show();
  }

  FastLED.clear();
  for (i = 0; i < TEST_LEDLEN; i++)
  {
    delay(10);
    leds[i] = CRGB( 0, 0, TEST_BRIGHTNESS);
    FastLED.show();
  }

  delay(500);
  FastLED.clear();
  for (i = 0; i < TEST_LEDLEN; i++)
  {
    leds[i] = CRGB( TEST_BRIGHTNESS, TEST_BRIGHTNESS, TEST_BRIGHTNESS);
  }
  FastLED.show();

  delay(1000);

  fastLedClear();
}


// データを受信したときに実行されるコールバック関数
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  if (busyF == true) {
    //書き換え中なので何もしない
    return;

  }

  memcpy(&myData, incomingData, sizeof(myData));

  M5.Speaker.beep();        // ビープ開始
  delay(100);               // 100ms鳴らす(beep()のデフォルト)
  M5.Speaker.mute();        //　ビープ停止

  //シリアル出力
  Serial.print(F("Bytes received: ")); Serial.println(len);
  Serial.print(F("ResetFlag : ")); Serial.println(myData.resetF);
  Serial.print(F("LedStep : ")); Serial.println(myData.LedStep);
  Serial.print(F("ledLen : ")); Serial.println(myData.ledLen);
  Serial.print(F("brightness : ")); Serial.println(myData.brightness);
  Serial.print(F("ledColor.length : "));  Serial.println(sizeof(myData.ledColor));

  //FastLEDライブラリを利用してWS2812BのLEDテープを表示
  CRGB leds[myData.ledLen];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, myData.ledLen).setCorrection(TypicalLEDStrip);
  FastLED.clear();

  if (myData.resetF == 1) //マイコンのリセット
  {
    FastLED.show();       //テープLEDに送信

    Serial.println(F("ESP.restart"));
    delay(50);
    ESP.restart();
  }

  uint16_t i;
  uint16_t p;
  p = 0;

  if (myData.resetF == 2) // LEDをフラグにあわせて表示
  {

    if (myData.LedStep == 0) {
      myData.LedStep = 1;
    }

    String test = "";
    for (i = 0; i <= myData.ledLen; i++)
    {
      if (i >= sizeof(myData.ledColor)) {
        break;
      }

      if (i > 0 && i % myData.LedStep != 0 )
      {
        leds[i] = CRGB( 0, 0, 0); //LED消灯
      }
      else
      {
        // LEDの色と明るさを設定
        if (myData.ledColor[p] == 'N') {
          leds[i] = CRGB( 0, 0, 0); //LED消灯
        }
        else if (myData.ledColor[p] == 'R') {
          leds[i] = CRGB( myData.brightness, 0, 0); //赤
        }
        else if (myData.ledColor[p] == 'G') {
          leds[i] = CRGB( 0, myData.brightness, 0); //緑
        }
        else if (myData.ledColor[p] == 'B') {
          leds[i] = CRGB( 0, 0, myData.brightness); //青
        }
        else if (myData.ledColor[p] == 'W') {
          leds[i] = CRGB( myData.brightness, myData.brightness, myData.brightness); //白
        }
        else {
          break;
        }
        test += myData.ledColor[p];
        p++;
      }
    }
    FastLED.show();
    Serial.println(test);
    Serial.println(F(""));

  }
  else if (myData.resetF == 3) // LED全消灯
  {
    FastLED.show();       //テープLEDに送信
    Serial.println(F("LED Clear"));
  }
  else if (myData.resetF == 4) // LED動作テスト(順次LEDを光らせる)
  {
    LedTest();
  }

  //M5StaLCD表示
  M5.Lcd.clear(); // 画面全体を消去
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);        M5.Lcd.println(F("Receive"));
  M5.Lcd.print(F("resetF  : ")); M5.Lcd.println(myData.resetF);
  M5.Lcd.print(F("LedStep : ")); M5.Lcd.println(myData.LedStep);
  M5.Lcd.print(F("ledLen  : ")); M5.Lcd.println(myData.ledLen);

  busyF = false;
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
    delay(500);
    ESP.restart();
  }
}

void setup()
{
  //引数(LCDを初期化,SDカードを使用するか, Serial通信をするか,I2C通信をするか)
  M5.begin(true, false, true, true);
  M5.Power.begin();

  M5.Speaker.begin();       // ノイズ(ポップ音)が出る
  M5.Speaker.setVolume(2);  // 0は無音、1が最小、8が初期値(結構大きい)

  fastLedClear(); // LED全消去

  //LCDの表示設定
  M5.Lcd.setBrightness(200); //バックライトの輝度（0~255)
  M5.Lcd.fillScreen(BLACK);  //背景色(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.setTextColor(WHITE, BLACK); //文字色設定と背景色設定
  M5.Lcd.setTextSize(3);     //文字サイズ（1～7）
  M5.Lcd.setCursor(0, 0);  M5.Lcd.println(F("Receive"));
  M5.Lcd.println(F("MAC Address"));  M5.Lcd.println(WiFi.macAddress());

  Serial.println(WiFi.macAddress()); // このアドレスを送信側へ登録します
  delay(50);

  //ESP-NOWを初期化
  InitESPNow();

  //ESP-NOWを受信したら"OnDataRecv"を起動
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  M5.update();  // ボタン操作の状況を読み込む関数(ボタン操作を行う際は必須)

  if (M5.BtnA.wasPressed()) // Aボタンが押された時:リセット
  {
    M5.Speaker.beep();        // ビープ開始
    delay(100);               // 100ms鳴らす(beep()のデフォルト)
    M5.Speaker.mute();        //　ビープ停止

    Serial.println(F("ESP32 Reset"));
    delay(50);
    esp_restart();
  }
  else if (M5.BtnB.wasPressed()) // Bボタンが押された時：LED消去
  {
    if (busyF != true) {
      busyF = true;
      M5.Speaker.beep();        // ビープ開始
      delay(100);               // 100ms鳴らす(beep()のデフォルト)
      M5.Speaker.mute();        //　ビープ停止

      fastLedClear(); // LED全消去
      M5.Lcd.clear(); // 画面全体を消去
      M5.Lcd.setCursor(0, 0);  M5.Lcd.println(F("Receive"));
      busyF = false;
    }

  }
  else if (M5.BtnC.wasPressed()) // Cボタンが押された時：テスト用
  {
    if (busyF != true) {
      busyF = true;
      M5.Lcd.clear(); // 画面全体を消去
      M5.Lcd.setCursor(0, 0); M5.Lcd.println(F("FastLED test"));

      Serial.println(F("FastLED test"));
      M5.Speaker.beep();        // ビープ開始
      delay(100);               // 100ms鳴らす(beep()のデフォルト)
      M5.Speaker.mute();        //　ビープ停止

      LedTest();
      M5.Lcd.clear(); // 画面全体を消去
      M5.Lcd.setCursor(0, 0);  M5.Lcd.println(F("Receive"));
      busyF = false;
    }
  }

  delay(100);
}
