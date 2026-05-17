#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <lvgl.h>

// Screen Manager and screens
#include "ui/screen_manager.h"
#include "ui/toast_manager.h"
#include "ui/screens/home_screen.h"
#include "ui/screens/about_screen.h"
#include "ui/screens/settings_screen.h"

// Launchers
void launchSettings() {
    ScreenManager::getInstance().navigateTo(
        new SettingsScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void launchAbout() {
    ScreenManager::getInstance().navigateTo(
        new AboutScreen(), LV_SCR_LOAD_ANIM_FADE_ON);
}

// Pinout and constants
#define TOUCH_CS_PIN  33
#define TOUCH_IRQ_PIN 36

#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 150
#define TOUCH_Y_MAX 3900

#define LV_BUF_SIZE (240 * 20)

// Global objects
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LV_BUF_SIZE];
static lv_color_t buf2[LV_BUF_SIZE];

// Display: reset hardware
void hardReset() {
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    delay(10);
    digitalWrite(4, LOW);
    delay(20);
    digitalWrite(4, HIGH);
    delay(150);
}

// LVGL callbacks
void flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

void readTouch(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    if (!touch.touched()) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    TS_Point p = touch.getPoint();

    int screen_x = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width() - 1);
    int screen_y = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height() - 1);

    data->state   = LV_INDEV_STATE_PR;
    data->point.x = constrain(screen_x, 0, tft.width() - 1);
    data->point.y = constrain(screen_y, 0, tft.height() - 1);
}

// LVGL task
void lvglTask(void* param) {
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// Setup
void setup() {
    Serial.begin(115200);
    Serial.println("[Pickle OS] Booting...");

    hardReset();

    // Display initialization
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    // Touchscreen initialization on own SPI bus
    SPI.begin(25, 39, 32, TOUCH_CS_PIN);
    touch.begin();
    touch.setRotation(0);

    // LVGL initialization
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 320;
    disp_drv.flush_cb = flushDisplay;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = readTouch;
    lv_indev_drv_register(&indev_drv);

    // Initialize Toast Manager
    ToastManager::getInstance().init();

    // Start Screen Manager with HomeScreen
    ScreenManager::getInstance().navigateTo(
        new HomeScreen(), LV_SCR_LOAD_ANIM_FADE_ON, 400);

    // Start LVGL on core 1 (core 0 free for WiFi/BT later)
    xTaskCreatePinnedToCore(lvglTask, "lvgl", 8192, NULL, 1, NULL, 1);

    Serial.println("[Pickle OS] Boot complete. LVGL running on core 1.");
}

// Loop: empty, todo corre en FreeRTOS tasks
void loop() {
    vTaskDelay(portMAX_DELAY);
}