#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../hal/module_manager.h"
#include "temp_hum_screen.h"

// ModulesScreen - Lists all installed hardware modules with their connector and status.
class ModulesScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildList();
    }

    void onResume() override {
        _refresh();
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
    lv_obj_t*   _list = nullptr;
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
        lv_label_set_text(title, "Modules");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildList() {
        _list = lv_obj_create(_screen);
        lv_obj_set_size(_list, 240, 280);
        lv_obj_set_pos(_list, 0, 40);
        lv_obj_set_style_bg_color(_list, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_list, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_row(_list, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);

        _refresh();

        _tickTimer = lv_timer_create([](lv_timer_t* t) {
            ((ModulesScreen*)t->user_data)->_refresh();
        }, 1500, this);
    }

    void _refresh() {
        if (!_list) return;
        lv_obj_clean(_list);

        ModuleManager& mgr = ModuleManager::getInstance();
        if (mgr.count() == 0) {
            lv_obj_t* empty = lv_label_create(_list);
            lv_label_set_text(empty, "No modules registered");
            lv_obj_set_style_text_color(empty, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(empty, gFontNormal, LV_PART_MAIN);
            return;
        }

        for (size_t i = 0; i < mgr.count(); i++) {
            _makeModuleCard(mgr.at(i));
        }
    }

    void _makeModuleCard(Module* mod) {
        if (!mod) return;

        lv_obj_t* card = lv_btn_create(_list);
        lv_obj_set_width(card, lv_pct(100));
        lv_obj_set_height(card, 72);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_bg_color(card, gTheme->primaryDark, LV_STATE_PRESSED);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* nameLbl = lv_label_create(card);
        lv_label_set_text(nameLbl, mod->name());
        lv_obj_set_style_text_color(nameLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(nameLbl, gFontNormal, LV_PART_MAIN);
        lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t* connLbl = lv_label_create(card);
        lv_label_set_text_fmt(connLbl, "Connector: %s",
            ModuleManager::connectorName(mod->connector()));
        lv_obj_set_style_text_color(connLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(connLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(connLbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        lv_obj_t* statusLbl = lv_label_create(card);
        bool ok = mod->isConnected();
        lv_label_set_text(statusLbl, ok ? "Connected" : "Not detected");
        lv_obj_set_style_text_color(statusLbl,
            ok ? gTheme->primary : gTheme->errorText, LV_PART_MAIN);
        lv_obj_set_style_text_font(statusLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(statusLbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        ModuleType t = mod->type();
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            ModuleType t = (ModuleType)(intptr_t)lv_event_get_user_data(e);
            if (t == ModuleType::TEMP_HUMIDITY) {
                ScreenManager::getInstance().navigateTo(
                    new TempHumScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else {
                ToastManager::getInstance().showToast("No app for this module yet",
                    ToastType::ALERT);
            }
        }, LV_EVENT_CLICKED, (void*)(intptr_t)t);
    }
};
