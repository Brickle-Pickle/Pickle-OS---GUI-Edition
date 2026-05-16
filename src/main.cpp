#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#define TOUCH_CS_PIN  33
#define TOUCH_IRQ_PIN 36

#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 150
#define TOUCH_Y_MAX 3900

XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);
TFT_eSPI tft = TFT_eSPI();

void hardReset() {
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    delay(10);
    digitalWrite(4, LOW);
    delay(20);
    digitalWrite(4, HIGH);
    delay(150);
}

void mapTouch(int raw_x, int raw_y, int* screen_x, int* screen_y) {
    *screen_x = map(raw_x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width() - 1);
    *screen_y = map(raw_y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height() - 1);
    *screen_x = constrain(*screen_x, 0, tft.width() - 1);
    *screen_y = constrain(*screen_y, 0, tft.height() - 1);
}

void setup() {
    Serial.begin(115200);
    hardReset();

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    SPI.begin(25, 39, 32, TOUCH_CS_PIN);
    touch.begin();
    touch.setRotation(0);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.println("Touch anywhere to test");
}

void loop() {
    if (!touch.touched()) return;

    TS_Point p = touch.getPoint();

    int screen_x, screen_y;
    mapTouch(p.x, p.y, &screen_x, &screen_y);

    tft.fillCircle(screen_x, screen_y, 4, TFT_RED);

    Serial.print("raw: ");
    Serial.print(p.x);
    Serial.print(",");
    Serial.print(p.y);
    Serial.print(" -> screen: ");
    Serial.print(screen_x);
    Serial.print(",");
    Serial.println(screen_y);

    delay(50);
}