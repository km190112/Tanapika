/*
  ESP-NOW受信側のプログラム
  ESP-NOWで受信した'struct_message'構造体からLED(WS2812b)のLEDを光らせる。(消灯,Red,Green,Blue,White)と明るさの調整が可能
  マイコンはESP32 DevkitCを使用
  ESP-NOWはWPA2標準で使用するための標準暗号化プロトコル
  ESP32の開発会社であるEspressifが開発した通信方式で、内容的にはIEEE 802.11のCCM Protocolを使って通信
*/

#include <WiFi.h>
#include <esp_now.h>
#include <FastLED.h>

#define SOFTWERE_VER 3.0

#define SERIAL_BPS 115200  // USBシリアル通信の速度bps

//FastLED
#define LED_PIN 26          // LEDの信号出力のピン番号
#define TEST_BRIGHTNESS 40  // テスト用 LEDの明るさ
#define MAX_LEDLEN 200      // 制御できるLEDの最大個数
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// データを受信するための構造
// 送信者の構造と一致する必要があります。ESP-NOWのペイロード長：205byte/最大250byte
typedef struct struct_message {
  uint8_t commandFlg;  // 1=マイコンリセット、2=LEDを点灯、3=LED全消灯,4=LED動作テスト
  uint8_t brightness;  // LEDの明るさ(0~255)
  uint16_t ledLen;     // 接続しているLEDの最大数。ledColorに格納したバイト数
  uint16_t LedStep;    // 利用するLEDの間隔。ledColorが5番目でLedStepが3の場合：16番目のLEDが点灯する
  char ledColor[201];  // LEDごとに先頭から光らせる色を指定(R,G,B,N,Wのいずれかを指定)最大200番目まで設定可
} struct_message;

struct_message resiveData;  // resiveDataというstruct_messageを作成

// 'commandFlg'の定数
#define COMMAND_FLG_RESET 1      // マイコンリセット
#define COMMAND_FLG_LIGHT_UP 2   // LEDを点灯
#define COMMAND_FLG_LIGHT_OFF 3  // LED全消灯
#define COMMAND_FLG_LED_TEST 4   // LED動作テスト


void fastLedClear() {
  //FastLEDでLED全消灯
  CRGB leds[MAX_LEDLEN];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, MAX_LEDLEN).setCorrection(TypicalLEDStrip);
  FastLED.clear();
  FastLED.show();
  delay(60);
  return;
}


void LedTest() {
  fastLedClear();

  //FastLEDのテスト
  CRGB leds[MAX_LEDLEN];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, MAX_LEDLEN).setCorrection(TypicalLEDStrip);
  FastLED.clear();

  uint8_t ledCnt;
  uint8_t loopCnt;

  FastLED.clear();

  for (loopCnt = 0; loopCnt < 8; loopCnt++) {
    for (ledCnt = 0; ledCnt < MAX_LEDLEN; ledCnt++) {

      switch (loopCnt) {
        case 1:
          leds[ledCnt] = CRGB(TEST_BRIGHTNESS, 0, 0);
          break;
        case 2:
          leds[ledCnt] = CRGB(TEST_BRIGHTNESS, TEST_BRIGHTNESS, 0);
          break;
        case 3:
          leds[ledCnt] = CRGB(0, TEST_BRIGHTNESS, 0);
          break;
        case 4:
          leds[ledCnt] = CRGB(0, 0, TEST_BRIGHTNESS);
          break;
        case 5:
          leds[ledCnt] = CRGB(TEST_BRIGHTNESS, 0, TEST_BRIGHTNESS);
          break;
        case 6:
          leds[ledCnt] = CRGB(0, TEST_BRIGHTNESS, TEST_BRIGHTNESS);
          break;
        case 7:
          leds[ledCnt] = CRGB(TEST_BRIGHTNESS, TEST_BRIGHTNESS, TEST_BRIGHTNESS);
      }

      FastLED.show();
      delay(10);
    }
  }

  delay(1000);
  FastLED.clear();

  delay(1000);

  fastLedClear();
  return;
}


// データを受信したときに実行されるコールバック関数
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&resiveData, incomingData, sizeof(resiveData));

  //シリアル出力
  // Serial.print(F("data received Len :"));
  // Serial.println(len);

  //FastLEDライブラリを利用してWS2812BのLEDテープを表示
  CRGB leds[resiveData.ledLen];
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, resiveData.ledLen).setCorrection(TypicalLEDStrip);
  FastLED.clear();

  uint8_t ledCnt;

  switch (resiveData.commandFlg) {
    case COMMAND_FLG_RESET:  //マイコンのリセット
      FastLED.show();        //テープLEDに送信

      Serial.println(F("ESP Restart"));
      delay(100);
      ESP.restart();
      break;

    case COMMAND_FLG_LIGHT_UP:  // LEDをフラグにあわせて表示

      if (resiveData.LedStep == 0) {
        resiveData.LedStep = 1;
      }

      // String strLEDColor = "";
      for (ledCnt = 0; ledCnt <= resiveData.ledLen; ledCnt++) {
        if (ledCnt >= sizeof(resiveData.ledColor)) {
          break;
        }

        if (ledCnt > 0 && ledCnt % resiveData.LedStep > 0) {
          leds[ledCnt] = CRGB(0, 0, 0);  //LED消灯

        } else {
          // LEDの色と明るさを設定
          if (resiveData.ledColor[ledCnt] == 'N') {
            leds[ledCnt] = CRGB(0, 0, 0);  //LED消灯
          } else if (resiveData.ledColor[ledCnt] == 'R') {
            leds[ledCnt] = CRGB(resiveData.brightness, 0, 0);  //赤
          } else if (resiveData.ledColor[ledCnt] == 'G') {
            leds[ledCnt] = CRGB(0, resiveData.brightness, 0);  //緑
          } else if (resiveData.ledColor[ledCnt] == 'B') {
            leds[ledCnt] = CRGB(0, 0, resiveData.brightness);  //青
          } else if (resiveData.ledColor[ledCnt] == 'W') {
            leds[ledCnt] = CRGB(resiveData.brightness, resiveData.brightness, resiveData.brightness);  //白
          } else {
            break;
          }

          // strLEDColor += resiveData.ledColor[ledCnt];
        }
      }
      FastLED.show();

      Serial.print(F("LED Light ON"));
      // Serial.println(strLEDColor);
      break;

    case COMMAND_FLG_LIGHT_OFF:  // LED全消灯
      FastLED.show();            //テープLEDに送信
      Serial.println(F("LED Light OFF"));
      break;

    case COMMAND_FLG_LED_TEST:  // LED動作テスト(順次LEDを光らせる)
      LedTest();
      Serial.println(F("LED Light Test"));
      break;
  }

  //シリアル出力
  // Serial.print(F("Bytes received: ")); Serial.println(len);
  // Serial.print(F("commandFlg : ")); Serial.println(resiveData.commandFlg);
  // Serial.print(F("LedStep : ")); Serial.println(resiveData.LedStep);
  // Serial.print(F("ledLen : ")); Serial.println(resiveData.ledLen);
  // Serial.print(F("brightness : ")); Serial.println(resiveData.brightness);
  // Serial.print(F("ledColor.length : "));  Serial.println(sizeof(resiveData.ledColor));
}


void setup() {
  delay(250);

  Serial.begin(SERIAL_BPS);
  delay(50);

  fastLedClear();  // LED全消去

  Serial.print(F("TanaPika Received"));
  Serial.print(F("Softwere Ver : "));  
  Serial.println(SOFTWERE_VER);

  Serial.print(F("MACAddress: "));
  Serial.println(WiFi.macAddress());  // このアドレスを送信側へ登録します

  //ESP-NOWを初期化
  WiFi.mode(WIFI_STA);  // デバイスをWi-Fiステーションとして設定する
  WiFi.disconnect();
  delay(100);
  if (esp_now_init() != ESP_OK) {
    Serial.println(F("ESPNow Init Failed"));
    delay(500);
    ESP.restart();
  }

  Serial.println(F("ESPNow Init Success"));

  //ESP-NOWを受信したら"OnDataRecv"を起動
  esp_now_register_recv_cb(OnDataRecv);
  Serial.println(F("ESP-NOW Start"));
}

void loop() {
  delay(1);
}
