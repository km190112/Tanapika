/*
  ESP-NOW受信側のプログラム
  受信した信号からWS2812b RGBLEDを光らせる
  ESP32 DevkitCを使用
  ESP-NOWはWPA2標準で使用するための標準暗号化プロトコル
  ESP32の開発会社であるEspressifが開発した通信方式で、内容的にはIEEE 802.11のCCM Protocolを使って通信
*/
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>

#define SERIAL_BPS 115200 //シリアル通信の速度bps

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

  delay(1000);

  fastLedClear();
}


// データを受信したときに実行されるコールバック関数
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len)
{
  memcpy(&myData, incomingData, sizeof(myData));

  //シリアル出力
  Serial.print(F("data received")); Serial.println(len);

  fastLedClear(); // LED全消去
  delay(50);


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


    Serial.println(F("LED Color:")); Serial.println(test);
  }
  else if (myData.resetF == 3) // LED全消灯
  {
    FastLED.show();       //テープLEDに送信
    Serial.println(F("LED Clear"));
  }
  else if (myData.resetF == 4) // LED動作テスト(順次LEDを光らせる)
  {
    Serial.println(F("LED Test"));
    LedTest();
  }

  //シリアル出力
  Serial.print(F("Bytes received: ")); Serial.println(len);
  Serial.print(F("ResetFlag : ")); Serial.println(myData.resetF);
  Serial.print(F("LedStep : ")); Serial.println(myData.LedStep);
  Serial.print(F("ledLen : ")); Serial.println(myData.ledLen);
  Serial.print(F("brightness : ")); Serial.println(myData.brightness);
  Serial.print(F("ledColor.length : "));  Serial.println(sizeof(myData.ledColor));
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
  Serial.begin(SERIAL_BPS);
  delay(50);

  fastLedClear(); // LED全消去

  Serial.println(WiFi.macAddress()); // このアドレスを送信側へ登録します

  //ESP-NOWを初期化
  InitESPNow();

  //ESP-NOWを受信したら"OnDataRecv"を起動
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println(F("ESP-NOW Start"));
}


void loop()
{
  delay(100);
}
