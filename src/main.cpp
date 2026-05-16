#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <lvgl.h>

#define TOUCH_CS_PIN  33
#define TOUCH_IRQ_PIN 36

#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3900
#define TOUCH_Y_MIN 150
#define TOUCH_Y_MAX 3900

#define LV_BUF_SIZE (240 * 20)

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen touch(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LV_BUF_SIZE];
static lv_color_t buf2[LV_BUF_SIZE];

void hardReset() {
    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);
    delay(10);
    digitalWrite(4, LOW);
    delay(20);
    digitalWrite(4, HIGH);
    delay(150);
}

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

    data->state = LV_INDEV_STATE_PR;
    data->point.x = constrain(screen_x, 0, tft.width() - 1);
    data->point.y = constrain(screen_y, 0, tft.height() - 1);
}

void lvglTask(void* param) {
    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
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

    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = flushDisplay;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = readTouch;
    lv_indev_drv_register(&indev_drv);

    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Pickle OS GUI\nLVGL OK");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    xTaskCreatePinnedToCore(lvglTask, "lvgl", 4096, NULL, 1, NULL, 1);

    Serial.println("LVGL running on core 1");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}