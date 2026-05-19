#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../network/wifi_manager.h"

// WifiScreen - WiFi status & options launched from the Home WiFi tile
// For now the only available option is "Sync clock"
class WifiScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);

        _buildHeader();
        _buildContent();
    }

    void onResume() override {
        _refreshStatus();
    }

    void onDestroy() override {
        if (_statusTimer) {
            lv_timer_del(_statusTimer);
            _statusTimer = nullptr;
        }
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_obj_t*   _statusLabel = nullptr;
    lv_obj_t*   _ssidLabel   = nullptr;
    lv_obj_t*   _ipLabel     = nullptr;
    lv_obj_t*   _clockLabel  = nullptr;
    lv_timer_t* _statusTimer = nullptr;

    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 40);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 36, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, gTheme->primary, LV_PART_MAIN);
        lv_obj_center(backIco);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontIcon, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildContent() {
        // Status card
        lv_obj_t* card = lv_obj_create(_screen);
        lv_obj_set_size(card, 220, 150);
        lv_obj_set_pos(card, 10, 50);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_row(card, 6, LV_PART_MAIN);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        _statusLabel = lv_label_create(card);
        lv_obj_set_style_text_color(_statusLabel, gTheme->textDark, LV_PART_MAIN);
        // Must be an icon-capable font: gFontNormal may be Indie Flower which
        // lacks the LV_SYMBOL_* glyphs, rendering them as missing/blank chars.
        lv_obj_set_style_text_font(_statusLabel, gFontIcon, LV_PART_MAIN);

        _ssidLabel = lv_label_create(card);
        lv_obj_set_style_text_color(_ssidLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_ssidLabel, gFontSmall, LV_PART_MAIN);

        _ipLabel = lv_label_create(card);
        lv_obj_set_style_text_color(_ipLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_ipLabel, gFontSmall, LV_PART_MAIN);

        _clockLabel = lv_label_create(card);
        lv_obj_set_style_text_color(_clockLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_clockLabel, gFontSmall, LV_PART_MAIN);

        // Sync clock button
        lv_obj_t* btnSync = lv_btn_create(_screen);
        lv_obj_set_size(btnSync, 220, 44);
        lv_obj_set_pos(btnSync, 10, 210);
        lv_obj_set_style_bg_color(btnSync, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btnSync, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(btnSync, 10, LV_PART_MAIN);
        lv_obj_add_event_cb(btnSync, [](lv_event_t* e) {
            if (!WifiManager::getInstance().connected()) {
                ToastManager::getInstance().showToast("WiFi not connected", ToastType::ALERT);
                return;
            }
            WifiManager::getInstance().syncClock();
            ToastManager::getInstance().showIconToast("Syncing clock...", LV_SYMBOL_REFRESH, ToastType::INFO);
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* btnLbl = lv_label_create(btnSync);
        lv_label_set_text(btnLbl, LV_SYMBOL_REFRESH " Sync clock (NTP)");
        lv_obj_set_style_text_color(btnLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(btnLbl, gFontIcon, LV_PART_MAIN);
        lv_obj_center(btnLbl);

        _refreshStatus();

        // Refresh every 2s — fast enough for responsive feedback while keeping
        // the redraw load low so WiFi traffic doesn't fight LVGL repaints.
        _statusTimer = lv_timer_create([](lv_timer_t* t) {
            ((WifiScreen*)t->user_data)->_refreshStatus();
        }, 2000, this);
    }

    // Set label text only when it actually differs — avoids triggering a redraw
    // every tick which is the main cause of visible flicker with WiFi running.
    static void _setIfChanged(lv_obj_t* lbl, const char* text) {
        const char* cur = lv_label_get_text(lbl);
        if (cur && strcmp(cur, text) == 0) return;
        lv_label_set_text(lbl, text);
    }

    void _refreshStatus() {
        if (!_statusLabel) return;
        WifiManager& w = WifiManager::getInstance();

        const char* statusText;
        lv_color_t  statusColor;
        if (!w.enabled()) {
            statusText  = LV_SYMBOL_CLOSE " Disabled";
            statusColor = gTheme->errorText;
        } else if (w.connected()) {
            statusText  = LV_SYMBOL_OK " Connected";
            statusColor = gTheme->successText;
        } else {
            statusText  = LV_SYMBOL_REFRESH " Connecting...";
            statusColor = gTheme->alertText;
        }
        if (strcmp(lv_label_get_text(_statusLabel), statusText) != 0) {
            lv_label_set_text(_statusLabel, statusText);
            lv_obj_set_style_text_color(_statusLabel, statusColor, LV_PART_MAIN);
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "SSID: %s",
                 w.ssid().length() ? w.ssid().c_str() : "(not set)");
        _setIfChanged(_ssidLabel, buf);

        snprintf(buf, sizeof(buf), "IP: %s",
                 w.connected() ? w.ip().c_str() : "--");
        _setIfChanged(_ipLabel, buf);

        char hhmm[8];
        getClockHHMM(hhmm, sizeof(hhmm));
        snprintf(buf, sizeof(buf), "Clock: %s", hhmm);
        _setIfChanged(_clockLabel, buf);
    }
};
