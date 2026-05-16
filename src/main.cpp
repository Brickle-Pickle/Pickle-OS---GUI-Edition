#include <Arduino.h>
#include <TFT_eSPI.h>

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

void setup() {
    Serial.begin(115200);
    hardReset();

    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 20);
    tft.println("Pickle OS");
    tft.setCursor(20, 50);
    tft.println("GUI");
    tft.setCursor(20, 80);
    tft.println("Display OK");

    Serial.print("Width: ");
    Serial.println(tft.width());
    Serial.print("Height: ");
    Serial.println(tft.height());
}

void loop() {
    static uint8_t color_index = 0;
    uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_MAGENTA, TFT_CYAN};

    tft.fillCircle(tft.width() / 2, tft.height() - 40, 20, colors[color_index]);
    color_index = (color_index + 1) % 6;

    delay(500);
}