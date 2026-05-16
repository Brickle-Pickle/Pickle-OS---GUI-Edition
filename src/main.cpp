#include <Arduino.h>
#include <lvgl.h>

void setup() {
    Serial.begin(115200);
    Serial.println("Pickle OS booting...");
    lv_init();
    Serial.println("LVGL initialized OK");
}

void loop() {
    lv_timer_handler();
    delay(5);
}