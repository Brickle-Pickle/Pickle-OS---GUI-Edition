#pragma once
#include "screen_base.h"
#include "../screen_manager.h"

#define VERSION "v0.1.3"

// Forward declarations for avoiding circular includes
class SettingsScreen;
class AboutScreen;

// HomeScreen - Home screen with app grid
class HomeScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);

        // Dark background
        lv_obj_set_style_bg_color(_screen, lv_color_hex(0x121212), LV_PART_MAIN);

        _buildHeader();
        _buildAppGrid();
        _buildFooter();
    }

    void onResume() override {
        // Update clock on resume
        _updateClock();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_obj_t* _clockLabel = nullptr;

    // Header with OS name and clock
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Pickle OS");
        lv_obj_set_style_text_color(title, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_LEFT_MID, 8, 0);

        // Clock (placeholder, updated on resume)
        _clockLabel = lv_label_create(header);
        lv_label_set_text(_clockLabel, "--:--");
        lv_obj_set_style_text_color(_clockLabel, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_align(_clockLabel, LV_ALIGN_RIGHT_MID, -8, 0);
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
        static auto cbAlerts = [](lv_event_t* e) {
            ToastManager::getInstance().showToast("Alerts comming soon!");
        };
        static auto cbWiFi = [](lv_event_t* e) {
            ToastManager::getInstance().showToast("WiFi coming soon!");
        };
        static auto cbFiles = [](lv_event_t* e) {
            ToastManager::getInstance().showToast("File manager coming soon!");
        };
        static auto cbGames = [](lv_event_t* e) {
            ToastManager::getInstance().showToast("Games coming soon!");
        };

        const AppIcon apps[] = {
            { LV_SYMBOL_SETTINGS, "Settings", cbSettings },
            { LV_SYMBOL_WIFI, "WiFi", cbWiFi },
            { LV_SYMBOL_SD_CARD, "Files", cbFiles },
            { LV_SYMBOL_BELL, "Alerts", cbAlerts },
            { LV_SYMBOL_PLAY, "Games", cbGames },
            { LV_SYMBOL_LIST, "About", cbAbout },
        };

        const int COLS = 3;
        const int ICON_SIZE = 64;
        const int GAP = 8;
        const int START_X = (240 - COLS * ICON_SIZE - (COLS - 1) * GAP) / 2;
        const int START_Y = 44;

        for (int i = 0; i < 6; i++) {
            int col = i % COLS;
            int row = i / COLS;
            int x = START_X + col * (ICON_SIZE + GAP);
            int y = START_Y + row * (ICON_SIZE + GAP + 4);

            lv_obj_t* btn = lv_btn_create(_screen);
            lv_obj_set_size(btn, ICON_SIZE, ICON_SIZE);
            lv_obj_set_pos(btn, x, y);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x313244), LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x45475A), LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN);
            lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_PART_MAIN);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

            // Icon
            lv_obj_t* ico = lv_label_create(btn);
            lv_label_set_text(ico, apps[i].icon);
            lv_obj_set_style_text_color(ico, lv_color_hex(0x89B4FA), LV_PART_MAIN);
            lv_obj_set_style_text_font(ico, &lv_font_montserrat_18, LV_PART_MAIN);
            lv_obj_align(ico, LV_ALIGN_TOP_MID, 0, 8);

            // App name
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, apps[i].name);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, LV_PART_MAIN);
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
        lv_obj_set_style_text_color(footer, lv_color_hex(0x585B70), LV_PART_MAIN);
        lv_obj_set_style_text_font(footer, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    void _updateClock() {
        // Placeholder: when NTP is implemented, put the real time here
        if (_clockLabel) {
            lv_label_set_text(_clockLabel, "00:00");
        }
    }
};
