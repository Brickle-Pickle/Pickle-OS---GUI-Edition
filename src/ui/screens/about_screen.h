#pragma once
#include "screen_base.h"
#include "../screen_manager.h"

// AboutScreen - About screen with system info
class AboutScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, lv_color_hex(0x121212), LV_PART_MAIN);

        _buildHeader();
        _buildContent();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    // Header
    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 40);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 36, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(backBtn, [](lv_event_t* e) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_center(backIco);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "About");
        lv_obj_set_style_text_color(title, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    //  Content with system info
    void _buildContent() {
        lv_obj_t* card = lv_obj_create(_screen);
        lv_obj_set_size(card, 220, 260);
        lv_obj_set_pos(card, 10, 48);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E2E), LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(card, 16, LV_PART_MAIN);
        lv_obj_set_style_pad_row(card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Logo / name
        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, "Pickle OS");
        lv_obj_set_style_text_color(name, lv_color_hex(0x89B4FA), LV_PART_MAIN);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_18, LV_PART_MAIN);

        lv_obj_t* edition = lv_label_create(card);
        lv_label_set_text(edition, "GUI Edition");
        lv_obj_set_style_text_color(edition, lv_color_hex(0xCDD6F4), LV_PART_MAIN);

        // Separator
        lv_obj_t* sep = lv_obj_create(card);
        lv_obj_set_size(sep, lv_pct(100), 1);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x45475A), LV_PART_MAIN);
        lv_obj_set_style_border_width(sep, 0, LV_PART_MAIN);

        // System info
        struct InfoRow { const char* key; const char* val; };
        const InfoRow rows[] = {
            { "Board", "ESP32-2432S028" },
            { "Display", "ST7789 2.8\" 240x320" },
            { "Touch", "XPT2046 resistive" },
            { "GUI", "LVGL 8.3" },
            { "RTOS", "FreeRTOS" },
            { "Author", "Brickle Pickle" },
            { "Dev", "Antonio G." },
            { "CLI version", "Pickle OS" },
        };

        for (auto& r : rows) {
            lv_obj_t* rowObj = lv_obj_create(card);
            lv_obj_set_size(rowObj, lv_pct(100), LV_SIZE_CONTENT);
            lv_obj_set_style_bg_opa(rowObj, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(rowObj, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(rowObj, 0, LV_PART_MAIN);
            lv_obj_clear_flag(rowObj, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* key = lv_label_create(rowObj);
            lv_label_set_text(key, r.key);
            lv_obj_set_style_text_color(key, lv_color_hex(0x585B70), LV_PART_MAIN);
            lv_obj_set_style_text_font(key, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_align(key, LV_ALIGN_LEFT_MID, 0, 0);

            lv_obj_t* val = lv_label_create(rowObj);
            lv_label_set_text(val, r.val);
            lv_obj_set_style_text_color(val, lv_color_hex(0xCDD6F4), LV_PART_MAIN);
            lv_obj_set_style_text_font(val, &lv_font_montserrat_10, LV_PART_MAIN);
            lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
        }
    }
};
