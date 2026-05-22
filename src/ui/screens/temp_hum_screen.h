#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../../hal/module_manager.h"
#include "../../hal/modules/temp_hum_module.h"

// TempHumScreen - Live readings for the temperature/humidity module on CN1.
class TempHumScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildContent();

        _refresh();
        _tickTimer = lv_timer_create([](lv_timer_t* t) {
            ((TempHumScreen*)t->user_data)->_refresh();
        }, 1000, this);
    }

    void onDestroy() override {
        if (_tickTimer) {
            lv_timer_del(_tickTimer);
            _tickTimer = nullptr;
        }
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_obj_t*   _tempLabel = nullptr;
    lv_obj_t*   _humLabel = nullptr;
    lv_obj_t*   _statusLabel = nullptr;
    lv_timer_t* _tickTimer = nullptr;

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
        lv_obj_add_event_cb(backBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, NULL);

        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, gTheme->primary, LV_PART_MAIN);
        lv_obj_center(backIco);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Temp & Humidity");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildContent() {
        lv_obj_t* card = lv_obj_create(_screen);
        lv_obj_set_size(card, 220, 220);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 52);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* connector = lv_label_create(card);
        lv_label_set_text(connector, "Connector: CN1");
        lv_obj_set_style_text_color(connector, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(connector, gFontSmall, LV_PART_MAIN);
        lv_obj_align(connector, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t* tempCap = lv_label_create(card);
        lv_label_set_text(tempCap, "Temperature");
        lv_obj_set_style_text_color(tempCap, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(tempCap, gFontNormal, LV_PART_MAIN);
        lv_obj_align(tempCap, LV_ALIGN_TOP_LEFT, 0, 26);

        _tempLabel = lv_label_create(card);
        lv_label_set_text(_tempLabel, "-- C");
        lv_obj_set_style_text_color(_tempLabel, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_tempLabel, gFontIconLarge, LV_PART_MAIN);
        lv_obj_align(_tempLabel, LV_ALIGN_TOP_LEFT, 0, 48);

        lv_obj_t* humCap = lv_label_create(card);
        lv_label_set_text(humCap, "Humidity");
        lv_obj_set_style_text_color(humCap, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(humCap, gFontNormal, LV_PART_MAIN);
        lv_obj_align(humCap, LV_ALIGN_TOP_LEFT, 0, 100);

        _humLabel = lv_label_create(card);
        lv_label_set_text(_humLabel, "-- %");
        lv_obj_set_style_text_color(_humLabel, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_humLabel, gFontIconLarge, LV_PART_MAIN);
        lv_obj_align(_humLabel, LV_ALIGN_TOP_LEFT, 0, 122);

        _statusLabel = lv_label_create(card);
        lv_label_set_text(_statusLabel, "Waiting for sensor...");
        lv_obj_set_style_text_color(_statusLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_statusLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_statusLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }

    void _refresh() {
        Module* m = ModuleManager::getInstance().findByType(ModuleType::TEMP_HUMIDITY);
        TempHumModule* sensor = static_cast<TempHumModule*>(m);
        if (!sensor) {
            lv_label_set_text(_statusLabel, "Module not registered");
            lv_obj_set_style_text_color(_statusLabel, gTheme->errorText, LV_PART_MAIN);
            return;
        }

        if (!sensor->isConnected() || isnan(sensor->temperatureC()) || isnan(sensor->humidity())) {
            lv_label_set_text(_tempLabel, "-- C");
            lv_label_set_text(_humLabel, "-- %");
            lv_label_set_text(_statusLabel, "Sensor not detected on CN1");
            lv_obj_set_style_text_color(_statusLabel, gTheme->errorText, LV_PART_MAIN);
            return;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f C", sensor->temperatureC());
        lv_label_set_text(_tempLabel, buf);
        snprintf(buf, sizeof(buf), "%.1f %%", sensor->humidity());
        lv_label_set_text(_humLabel, buf);
        lv_label_set_text(_statusLabel, "Connected on CN1");
        lv_obj_set_style_text_color(_statusLabel, gTheme->primary, LV_PART_MAIN);
    }
};
