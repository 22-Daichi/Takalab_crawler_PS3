#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define rightPwmCh 0
#define leftPwmCh 1

const char *SSID = "FS050W_1_DA85A1";
const char *PASS = "34MG4D9B4DnT";
const uint16_t PORT = 50007;
WiFiUDP udp;
char rxbuf[256];

// タイムアウト設定
const uint32_t TIMEOUT_MS = 500; // 受信停止とみなす時間
uint32_t last_rx_ms = 0;
bool timeout_active = false; // 連続発火防止

char btn[64];

int sz, len, now, L;

bool dpad_up = false, dpad_down = false, dpad_left = false, dpad_right = false, btn_x = false;

// DパッドのBTN中の位置（0始まり）
const int IDX_UP = 8;     // ↑
const int IDX_DOWN = 9;   // ↓
const int IDX_LEFT = 10;  // ←
const int IDX_RIGHT = 11; // →

volatile unsigned long lastInterruptTime = 0;
volatile unsigned long currentTime = 0;

const int leftWheelPwrPin = 5;
const int rightWheelPwrPin = 16; // 強すぎ
const int leftWheelDirPin = 17;
const int rightWheelDirPin = 4;

const int inputPin = 25;  // 入力ピン（pullvdown）
const int outputPin = 12; // 出力ピン

int rightWheelPwr = 0;
int leftWheelPwr = 0;
bool rightWheelDir = 0;
bool leftWheelDir = 0;

int maxPwr = 200;

int t = 0;

volatile bool triggered = true;

// BTN=...;AX=...;SEQ=... から BTN だけ取り出す（簡易）
bool get_btn_field(const char *src, char *out, size_t outsz)
{
  const char *p = strstr(src, "BTN=");
  if (!p)
    return false;
  p += 4;
  size_t i = 0;
  while (*p && *p != ';' && i + 1 < outsz)
    out[i++] = *p++;
  out[i] = 0;
  return i > 0;
}

void pinModeSetup()
{
  pinMode(rightWheelPwrPin, OUTPUT);
  pinMode(rightWheelDirPin, OUTPUT);
  pinMode(leftWheelPwrPin, OUTPUT);
  pinMode(leftWheelDirPin, OUTPUT);
  digitalWrite(rightWheelPwrPin, LOW);
  digitalWrite(leftWheelPwrPin, LOW);
  pinMode(inputPin, INPUT_PULLDOWN); // プルアップ入力
  pinMode(outputPin, OUTPUT);        // 出力モード
  digitalWrite(outputPin, LOW);      // 初期はLOW
}

void pwmSetup()
{
  ledcSetup(0, 12800, 8);                      // チャンネル0、キャリア周波数1kHz、8ビットレンジ
  ledcAttachPin(rightWheelPwrPin, rightPwmCh); // PWMピンにチャンネル0を指定
  ledcSetup(1, 12800, 8);                      // チャンネル1、キャリア周波数1kHz、16ビットレンジ
  ledcAttachPin(leftWheelPwrPin, leftPwmCh);   // PWMピンにチャンネル1を指定
}

void onConnect()
{
  Serial.println("Connected!.");
}

void setup()
{
  pinModeSetup();
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(300);
  }
  udp.begin(PORT);
  // Ps3.attach(notify);
  // Ps3.attachOnConnect(onConnect);
  // Ps3.begin("5c:6d:20:2b:b2:f9"); // 9c:9c:1f:d0:04:be
  pwmSetup();
}

void WheelPwrOn()
{
  if (rightWheelPwr > 0)
  {
    rightWheelDir = 0;
    ledcWrite(rightPwmCh, rightWheelPwr);
    digitalWrite(rightWheelDirPin, rightWheelDir);
  }
  else
  {
    rightWheelDir = 1;
    ledcWrite(rightPwmCh, rightWheelPwr * (-1));
    digitalWrite(rightWheelDirPin, rightWheelDir);
  }
  if (leftWheelPwr > 0)
  {
    leftWheelDir = 0;
    ledcWrite(leftPwmCh, leftWheelPwr);
    digitalWrite(leftWheelDirPin, leftWheelDir);
  }
  else
  {
    leftWheelDir = 1;
    ledcWrite(leftPwmCh, leftWheelPwr * (-1));
    digitalWrite(leftWheelDirPin, leftWheelDir);
  }
  Serial.println(rightWheelPwr);
}

void WheelPwrOff()
{
  rightWheelPwr = 0;
  leftWheelPwr = 0;
  ledcWrite(rightPwmCh, 0);
  ledcWrite(leftPwmCh, 0);
}

void getWheelPwr()
{
  if (dpad_up)
  {
    rightWheelPwr += 5;
    leftWheelPwr += 5;
  }
  if (dpad_down)
  {
    rightWheelPwr -= 5;
    leftWheelPwr -= 5;
  }
  if (dpad_right)
  {
    rightWheelPwr -= 5;
    leftWheelPwr += 5;
  }
  if (dpad_left)
  {
    rightWheelPwr += 5;
    leftWheelPwr -= 5;
  }
  if (btn_x)
  {
    WheelPwrOff();
  }
}

void setWheelPwr()
{
  if (rightWheelPwr > maxPwr)
  {
    rightWheelPwr = maxPwr;
  }
  if (rightWheelPwr < -maxPwr)
  {
    rightWheelPwr = -maxPwr;
  }

  if (leftWheelPwr > maxPwr)
  {
    leftWheelPwr = maxPwr;
  }
  if (leftWheelPwr < -maxPwr)
  {
    leftWheelPwr = -maxPwr;
  }
}

void emergency()
{
  triggered = true;
  digitalWrite(outputPin, LOW);
  WheelPwrOff();
}

void onTimeout()
{
  // 例：安全側に全てLOW（アクティブHigh前提）
  WheelPwrOff();
}

void getValue()
{
  // ★ここを "文字 == '1'" にする（範囲チェックも一緒に）
  btn_x = (L > 0 && btn[0] == '1');
  dpad_up = (L > 8 && btn[8] == '1');
  dpad_down = (L > 9 && btn[9] == '1');
  dpad_left = (L > 10 && btn[10] == '1');
  dpad_right = (L > 11 && btn[11] == '1');
}

void loop()
{
  // 緊急停止関連
  if (digitalRead(inputPin) == 0) // スイッチが押された
  {
    emergency();
  }
  if (triggered && digitalRead(inputPin) == HIGH) // スイッチ押されてない
  {
    triggered = false;
    digitalWrite(outputPin, HIGH);
  }
  // 通信が来ているか
  now = millis();
  if (!timeout_active && (now - last_rx_ms > TIMEOUT_MS))
  {
    timeout_active = true;
    onTimeout();
  }
  // データ読み取り
  sz = udp.parsePacket();
  if (sz <= 0)
    return;
  len = udp.read((uint8_t *)rxbuf, min(sz, (int)sizeof(rxbuf) - 1));
  if (len <= 0)
    return;
  rxbuf[len] = 0;

  last_rx_ms = now;
  timeout_active = false;

  if (!get_btn_field(rxbuf, btn, sizeof(btn)))
    return;

  L = strlen(btn);

  getValue();

  if (timeout_active == 0)
  {
    getWheelPwr();
    setWheelPwr();
    WheelPwrOn();
  }
  delay(50);
}