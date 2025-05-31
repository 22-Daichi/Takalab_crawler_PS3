#include <Arduino.h>
#include <Ps3Controller.h>

#define rightPwmCh 0
#define leftPwmCh 1

volatile unsigned long lastInterruptTime = 0;
volatile unsigned long currentTime = 0;

/* const int rightWheelPwrPin = 5;
const int leftWheelPwrPin = 16; // 強すぎ
const int rightWheelDirPin = 17;
const int leftWheelDirPin = 4; */

const int rightWheelPwrPin = 5;
const int leftWheelPwrPin = 16; // 強すぎ
const int rightWheelDirPin = 17;
const int leftWheelDirPin = 4;

const int inputPin = 15; // 入力ピン（pullvdown）
const int outputPin = 2; // 出力ピン

int rightWheelPwr = 0;
int leftWheelPwr = 0;
bool rightWheelDir = 0;
bool leftWheelDir = 0;

int maxPwr = 50;

int t = 0;

volatile bool triggered = true;

void IRAM_ATTR handleInterrupt()
{
  currentTime = millis();
  if (currentTime - lastInterruptTime > 50)
  { // 50ms以上の間隔のみ有効
    triggered = true;
    digitalWrite(outputPin, LOW);
    lastInterruptTime = currentTime;
  }
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
  Serial.begin(9600);
  // Ps3.attach(notify);
  Ps3.attachOnConnect(onConnect);
  Ps3.begin("5c:6d:20:2b:b2:f9"); // 9c:9c:1f:d0:04:be
  Serial.println("Ready.");
  pwmSetup();
  attachInterrupt(digitalPinToInterrupt(inputPin), handleInterrupt, FALLING);
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
  if (Ps3.data.button.up)
  {
    rightWheelPwr += 5;
    leftWheelPwr += 5;
  }
  if (Ps3.data.button.down)
  {
    rightWheelPwr -= 5;
    leftWheelPwr -= 5;
  }
  if (Ps3.data.button.right)
  {
    rightWheelPwr -= 5;
    leftWheelPwr += 5;
  }

  if (Ps3.data.button.left)
  {
    rightWheelPwr += 5;
    leftWheelPwr -= 5;
  }
  if (Ps3.data.button.cross)
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

void loop()
{
  if (triggered && digitalRead(inputPin) == HIGH) // スイッチ押されてない
  {
    triggered = false;
    digitalWrite(outputPin, HIGH);
  }
  if (Ps3.isConnected())
  {
    getWheelPwr();
    setWheelPwr();
    WheelPwrOn();
  }
  if (!Ps3.isConnected())
  {
    Serial.println("NoConnection");
    // wheelPwrOff();
  }
  delay(50);
}