#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../toast/toast_manager.h"
#include "../theme/theme.h"
#include "../../network/wifi_manager.h"

#define VERSION "v1.2.0"

// Forward declarations for avoiding circular includes
class SettingsScreen;
class AboutScreen;

// HomeScreen - Home screen with app grid
class HomeScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);

        // Dark background
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);

        _buildHeader();
        _buildAppGrid();
        _buildFooter();
    }

    void onResume() override {
        // Update clock on resume
        _updateClock();
    }

    void onDestroy() override {
        if (_clockTimer) {
            lv_timer_del(_clockTimer);
            _clockTimer = nullptr;
        }
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_obj_t*   _clockLabel = nullptr;
    lv_timer_t* _clockTimer = nullptr;

    // Header with OS name and clock
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Pickle OS");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

        // Clock (real time once NTP has synced, "--:--" until then)
        _clockLabel = lv_label_create(header);
        lv_label_set_text(_clockLabel, "--:--");
        lv_obj_set_style_text_color(_clockLabel, gTheme->primary, LV_PART_MAIN);
        lv_obj_align(_clockLabel, LV_ALIGN_RIGHT_MID, -8, 0);

        // Refresh the clock every 10s so the displayed time stays current
        _clockTimer = lv_timer_create([](lv_timer_t* t) {
            ((HomeScreen*)t->user_data)->_updateClock();
        }, 10000, this);
    }

    // 2x3 grid of app icons
    void _buildAppGrid() {
        // App icons data: { emoji/text, label, callback }
        struct AppIcon {
            const char* icon;
            const char* name;
            lv_event_cb_t cb;
        };

        // Static callbacks (necessary for lv_event_cb_t)
        static auto cbSettings = [](lv_event_t* e) {
            extern void launchSettings();
            launchSettings();
        };
        static auto cbAbout = [](lv_event_t* e) {
            extern void launchAbout();
            launchAbout();
        };
        static auto cbWiFi = [](lv_event_t* e) {
            extern void launchWifi();
            launchWifi();
        };
        static auto cbFiles = [](lv_event_t* e) {
            extern void launchFiles();
            launchFiles();
        };
        static auto cbGames = [](lv_event_t* e) {
            extern void launchGames();
            launchGames();
        };
        static auto cbClock = [](lv_event_t* e) {
            extern void launchClock();
            launchClock();
        };
        static auto cbModules = [](lv_event_t* e) {
            extern void launchModules();
            launchModules();
        };
        static auto cbHttp = [](lv_event_t* e) {
            extern void launchHttpTester();
            launchHttpTester();
        };
        static auto cbTotp = [](lv_event_t* e) {
            extern void launchTotp();
            launchTotp();
        };
        static auto cbBrowser = [](lv_event_t* e) {
            extern void launchBrowser();
            launchBrowser();
        };

        const AppIcon apps[] = {
            { LV_SYMBOL_SETTINGS, "Settings", cbSettings },
            { LV_SYMBOL_WIFI, "WiFi", cbWiFi },
            { LV_SYMBOL_SD_CARD, "Files", cbFiles },
            { LV_SYMBOL_DOWNLOAD, "Browser", cbBrowser },
            { LV_SYMBOL_PLAY, "Games", cbGames },
            { LV_SYMBOL_LIST, "About", cbAbout },
            { LV_SYMBOL_LOOP, "Clock", cbClock },
            { LV_SYMBOL_USB, "Modules", cbModules },
            { LV_SYMBOL_UPLOAD, "HTTP", cbHttp },
            { "2FA", "Auth", cbTotp },
        };
        const int APP_COUNT = sizeof(apps) / sizeof(apps[0]);

        const int COLS = 3;
        const int ICON_SIZE = 64;
        const int GAP = 8;
        const int START_X = (240 - COLS * ICON_SIZE - (COLS - 1) * GAP) / 2;
        const int START_Y = 44;

        for (int i = 0; i < APP_COUNT; i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = START_X + col * (ICON_SIZE + GAP);
            int y = START_Y + row * (ICON_SIZE + GAP + 4);

            lv_obj_t* btn = lv_btn_create(_screen);
            lv_obj_set_size(btn, ICON_SIZE, ICON_SIZE);
            lv_obj_set_pos(btn, x, y);
            lv_obj_set_style_bg_color(btn, gTheme->primaryDark, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, gTheme->primary, LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_PART_MAIN);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

            // Icon
            lv_obj_t* ico = lv_label_create(btn);
            lv_label_set_text(ico, apps[i].icon);
            lv_obj_set_style_text_color(ico, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(ico, gFontIconLarge, LV_PART_MAIN);
            lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 8);

            // App name
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, apps[i].name);
            lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
            lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -4);

            if (apps[i].cb) {
                lv_obj_add_event_cb(btn, apps[i].cb, LV_EVENT_CLICKED, NULL);
            }
        }
    }

    // Footer with version
    void _buildFooter() {
        lv_obj_t* footer = lv_label_create(_screen);
        lv_label_set_text(footer, VERSION);
        lv_obj_set_style_text_color(footer, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(footer, gFontSmall, LV_PART_MAIN);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    void _updateClock() {
        if (!_clockLabel) return;
        char hhmm[8];
        getClockHHMM(hhmm, sizeof(hhmm));
        // Skip the redraw entirely if the minute hasn't ticked over
        const char* cur = lv_label_get_text(_clockLabel);
        if (cur && strcmp(cur, hhmm) == 0) return;
        lv_label_set_text(_clockLabel, hhmm);
    }
};
