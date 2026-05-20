#include <Arduino.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <lvgl.h>

// Screen Manager and screens
#include "ui/screen_manager.h"
#include "ui/toast/toast_manager.h"
#include "ui/theme/theme.h"
#include "ui/screens/home_screen.h"
#include "ui/screens/about_screen.h"
#include "ui/screens/settings_screen.h"
#include "ui/screens/splash_screen.h"
#include "ui/screens/files_screen.h"
#include "ui/screens/wifi_screen.h"
#include "ui/screens/clock_screen.h"
#include "ui/screens/games_screen.h"
#include "ui/brightness.h"

// Storage manager
#include "storage/fs_manager.h"
#include "storage/sd_manager.h"
#include "storage/config_store.h"

// Network
#include "network/wifi_manager.h"

// Launchers
void launchSettings() {
    ScreenManager::getInstance().navigateTo(
        new SettingsScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void launchAbout() {
    ScreenManager::getInstance().navigateTo(
        new AboutScreen(), LV_SCR_LOAD_ANIM_FADE_ON);
}
void launchFiles() {
    ScreenManager::getInstance().navigateTo(
        new FilesScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void launchWifi() {
    ScreenManager::getInstance().navigateTo(
        new WifiScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void launchClock() {
    ScreenManager::getInstance().navigateTo(
        new ClockScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void launchGames() {
    ScreenManager::getInstance().navigateTo(
        new GamesScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
}
void resetToHome() {
    ScreenManager::getInstance().replaceRoot(new HomeScreen());
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
SPIClass sdSPI(HSPI);  // HSPI free for SD; VSPI is used by display+touch

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
    Brightness::init();

    // Configure VSPI for the touch FIRST. SPIClass::begin() is a no-op if the bus
    // was already started, so we must set the CYD touch pins before touch.begin()
    // triggers its internal SPI.begin() (which would lock VSPI to default pins).
    SPI.begin(25, 39, 32);
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

    // Initialize SD card on HSPI (pins 18/19/23) to avoid clobbering VSPI touch config
    sdSPI.begin(18, 19, 23, 5);  // CLK, MISO, MOSI, CS
    SDManager::getInstance().init(5, sdSPI);

    // Make sure the main path ("/pickle-os") exists on SD card for file operations
    if (!SDManager::getInstance().exists("/pickle-os")) {
        SDManager::getInstance().makeDir("/pickle-os");
    }

    // Check for settings file on the SD card to load Brightness, wifi credentials, host...
    if (!SDManager::getInstance().exists("/pickle-os/sys")) {
        SDManager::getInstance().makeDir("/pickle-os/sys");
    }
    if (!SDManager::getInstance().exists("/pickle-os/sys/config.txt")) {
        // Create file with defaults
        SDManager::getInstance().writeFile(
            "/pickle-os/sys/config.txt",
            "brightness=255\n"
            "wifi_ssid=\n"
            "wifi_pass=\n"
            "wifi_enabled=0\n"
            "hostname=pickle-os\n"
        );
        Serial.println("[Pickle OS] No config file found, using defaults.");
    }
    // Load config into memory so WifiManager (and future modules) can read it
    ConfigStore::getInstance().load();

    // Apply persisted brightness now that config is in memory
    Brightness::load();

    if (SDManager::getInstance().isMounted()) {
        Serial.println("[Pickle OS] SD card mounted successfully.");
    } else {
        Serial.println("[Pickle OS] Warning: SD card failed to mount.");
        ToastManager::getInstance().showToast("SD card failed to mount!", ToastType::ALERT);
    }

    // Load persisted theme and font before building any screen
    loadTheme();
    loadFont();

    // Initialize WiFi — loads saved credentials and auto-connects if enabled.
    // On successful connection, WifiManager triggers an NTP sync automatically.
    WifiManager::getInstance().begin();

    // Start with SplashScreen, auto-navigates to HomeScreen after 2.5s
    ScreenManager::getInstance().navigateTo(
        new SplashScreen(), LV_SCR_LOAD_ANIM_FADE_ON, 400);

    // Start LVGL on core 1 (core 0 free for WiFi/BT later)
    xTaskCreatePinnedToCore(lvglTask, "lvgl", 8192, NULL, 1, NULL, 1);

    Serial.println("[Pickle OS] Boot complete. LVGL running on core 1.");
}

// Loop: empty, todo corre en FreeRTOS tasks
void loop() {
    vTaskDelay(portMAX_DELAY);
}