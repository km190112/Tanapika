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
#define LED_PIN           26 //LEDの信号出力のピン番号
#define TEST_BRIGHTNESS  100 //テスト用 LEDの明るさ
#define TEST_LEDLEN       50 //テスト用 LEDの個数
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
  String ledColor;   // LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;

// myDataというstruct_messageを作成します
struct_message myData;

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

void LedTest(uint16_t ledLen) {
  //FastLEDのテスト
  CRGB leds[TEST_LEDLEN];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, ledLen).setCorrection(TypicalLEDStrip);

  
  FastLED.clear();
  
  for (uint16_t i = 0; i < ledLen; i++)
  {
    leds[i] = CRGB( TEST_BRIGHTNESS, 0, 0);
    FastLED.show();
    delay(150);
  }

  FastLED.clear();
  for (i = 0; i < ledLen; i++)
  {
    leds[i] = CRGB( 0, TEST_BRIGHTNESS, 0);
    FastLED.show();
    delay(120);
  }
  FastLED.clear();
  for (i = 0; i < ledLen; i++)
  {
    leds[i] = CRGB( 0, 0, TEST_BRIGHTNESS);
    FastLED.show();
    delay(80);
  }

  FastLED.clear();
  for (i = 0; i < ledLen; i++)
  {

    leds[i] = CRGB( TEST_BRIGHTNESS, TEST_BRIGHTNESS, TEST_BRIGHTNESS);

    FastLED.show();
    delay(80);
  }

  //LEDを消灯
  FastLED.clear();
  FastLED.show();
}



// データを受信したときに実行されるコールバック関数
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  memcpy(&myData, incomingData, sizeof(myData));

  //シリアル出力
  //Serial.print(F("MAC Address: ")); Serial.println(mac);
  Serial.print(F("Bytes received: ")); Serial.println(len);
  Serial.print(F("ResetFlag : ")); Serial.println(myData.resetF);
  Serial.print(F("LedStep : ")); Serial.println(myData.LedStep);
  Serial.print(F("ledLen : ")); Serial.println(myData.ledLen);
  Serial.print(F("brightness : ")); Serial.println(myData.brightness);
  Serial.print(F("ledColor : ")); Serial.println(myData.ledColor);

  if (myData.resetF == 1) //マイコンのリセット
  {
    FastLED.clear();
    FastLED.show();       //テープLEDに送信
    Serial.print(F("ESP.restart"));
    delay(1000);
    ESP.restart();
  }

  //FastLEDライブラリを利用してWS2812BのLEDテープを表示
  CRGB leds[myData.ledLen];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, myData.ledLen).setCorrection(TypicalLEDStrip);
  uint16_t i;

  FastLED.clear();
  if (myData.resetF == 2) // LEDをフラグにあわせて表示
  {
    char coler;

    for (i = 0; i < myData.ledLen; i++)
    {
      if (i > 0 && i % myData.LedStep != 0 || i >= myData.ledColor.length() - 1)
      {
        leds[i] = CRGB( 0, 0, 0); //LED消灯
      }

      else
      {
        coler = char(myData.ledColor.charAt(i));

        // LEDの色と明るさを設定
        if (coler == 'N') {
          leds[i] = CRGB( 0, 0, 0); //LED消灯
        }
        else if (coler == 'R') {
          leds[i] = CRGB( myData.brightness, 0, 0); //赤
        }
        else if (coler == 'G') {
          leds[i] = CRGB( 0, myData.brightness, 0); //緑
        }
        else if (coler == 'B') {
          leds[i] = CRGB( 0, 0, myData.brightness); //青
        }
        else if (coler == 'W') {
          leds[i] = CRGB( myData.brightness, myData.brightness, myData.brightness); //白
        }
        else {
          leds[i] = CRGB( 0, 0, 0); //LED消灯
        }
      }
    }
    FastLED.show();
  }
  else if (myData.resetF > 3) // LED全消灯
  {
    for (i = 0; i < TEST_LEDLEN; i++)
    {
      leds[i] = CRGB( 0, 0, 0);

    }
    FastLED.show();

  }
  else if (myData.resetF > 4) // LED動作テスト(順次LEDを光らせる)
  {
    LedTest(TEST_LEDLEN);
  }
  //M5StaLCD表示
  M5.Lcd.clear(); // 画面全体を消去
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(5, 5); M5.Lcd.println("Resive");
  M5.Lcd.println(myData.resetF);
  M5.Lcd.println(myData.LedStep);
  M5.Lcd.println(myData.ledLen);
}


void setup()
{
  //引数(LCDを初期化,SDカードを使用するか, Serial通信をするか,I2C通信をするか)
  M5.begin(true, false, true, true);
  M5.Power.begin();

  M5.Speaker.begin();       // ノイズ(ポップ音)が出る
  M5.Speaker.setVolume(1);  // 0は無音、1が最小、8が初期値(結構大きい)

  M5.Lcd.setBrightness(200); //バックライトの輝度（0~255)
  M5.Lcd.fillScreen(BLACK);  //背景色(WHITE, BLACK, RED, GREEN, BLUE, YELLOW...)
  M5.Lcd.setTextColor(WHITE, BLACK); //文字色設定と背景色設定
  M5.Lcd.setTextSize(3);     //文字サイズ（1～7）
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("MAC Address:");
  M5.Lcd.println(WiFi.macAddress());

  Serial.println(WiFi.macAddress()); // このアドレスを送信側へ登録します
  delay(50);

  //ESP-NOWを初期化
  InitESPNow();

  //ESP-NOWを受信したら"OnDataRecv"を起動
  esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
  delay(100);
  M5.update();  // ボタン操作の状況を読み込む関数(ボタン操作を行う際は必須)

  if (M5.BtnA.wasPressed()) // Aボタンが押された時:リセット
  {
    Serial.println("Reset");
    delay(100);
    esp_restart();
  }
  else if (M5.BtnB.wasPressed()) // Bボタンが押された時：画面暗くして省電力モード
  {
    M5.Lcd.clear(); // 画面全体を消去
    M5.Lcd.setBrightness(0); //バックライトの輝度（0~255)
  }
  else if (M5.BtnC.wasPressed()) // Cボタンが押された時：テスト用
  {
    Serial.println("FastLED test");
    M5.Speaker.beep();        // ビープ開始
    delay(100);               // 100ms鳴らす(beep()のデフォルト)
    M5.Speaker.mute();        //　ビープ停止

    LedTest(TEST_LEDLEN);
  }
}
