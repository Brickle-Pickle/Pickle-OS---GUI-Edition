#pragma once
#include <lvgl.h>
#include <ArduinoJson.h>
#include "../screen_base.h"
#include "../../screen_manager.h"
#include "../../theme/theme.h"
#include "../../toast/toast_manager.h"
#include "game_over_modal.h"
#include "../../../storage/game_installer.h"
#include "../../../scripting/lua_engine.h"

// LuaGameScreen - host shell for any downloaded Lua game.
//
// Layout: a 36 px header (back button, HUD left, HUD right) and a fullscreen
// "area" view exposed to the script as the LVGL parent for any widget or
// canvas it creates. The script can update both HUD slots via game.set_hud().
class LuaGameScreen : public ScreenBase {
public:
    explicit LuaGameScreen(const String& id) : _id(id) {}

    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        if (!_loadManifest()) {
            _buildHeader("Error");
            _buildError("Invalid manifest.");
            return;
        }

        String script = SDManager::getInstance().readFile(_scriptPath().c_str());
        if (!script.length()) {
            _buildHeader(_name.c_str());
            _buildError("Script not found.");
            return;
        }
        _buildHeader(_name.c_str());

        _engine = new LuaEngine(_area,
            [this](const char* l, const char* r) { _onHud(l, r); },
            [this](int score) { _onGameOver(score); });

        if (!_engine->run(script)) {
            _buildError("Lua execution failed.");
        }
    }

    void onDestroy() override {
        if (_engine) { delete _engine; _engine = nullptr; }
        if (_screen) { lv_obj_del(_screen); _screen = nullptr; }
        _area = _hudLeft = _hudRight = nullptr;
    }

private:
    String _id;
    String _name;
    String _entry = "game.lua";
    LuaEngine* _engine = nullptr;
    lv_obj_t* _area = nullptr;
    lv_obj_t* _hudLeft = nullptr;
    lv_obj_t* _hudRight = nullptr;
    GameOverModal _modal;

    String _scriptPath() {
        return String(GameInstaller::ROOT) + "/" + _id + "/" + _entry;
    }

    bool _loadManifest() {
        String json = GameInstaller::getInstance().readManifest(_id);
        if (!json.length()) return false;
        JsonDocument doc;
        if (deserializeJson(doc, json)) return false;
        _name = String((const char*)(doc["name"] | _id.c_str()));
        _entry = String((const char*)(doc["entry"] | "game.lua"));
        return true;
    }

    void _buildHeader(const char* title) {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 32, 28);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(backBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(backBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(backBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ico = lv_label_create(backBtn);
        lv_label_set_text(ico, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(ico, gTheme->textDark, LV_PART_MAIN);
        lv_obj_center(ico);
        lv_obj_add_event_cb(backBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        _hudLeft = lv_label_create(header);
        lv_label_set_text(_hudLeft, title);
        lv_obj_set_style_text_color(_hudLeft, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_hudLeft, gFontNormal, LV_PART_MAIN);
        lv_obj_align(_hudLeft, LV_ALIGN_CENTER, 0, 0);

        _hudRight = lv_label_create(header);
        lv_label_set_text(_hudRight, "");
        lv_obj_set_style_text_color(_hudRight, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_hudRight, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_hudRight, LV_ALIGN_RIGHT_MID, -8, 0);

        _area = lv_obj_create(_screen);
        lv_obj_set_size(_area, 240, 320 - 36);
        lv_obj_set_pos(_area, 0, 36);
        lv_obj_set_style_bg_color(_area, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_area, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_area, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_area, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_area, LV_OBJ_FLAG_SCROLLABLE);
    }

    void _buildError(const char* msg) {
        if (!_area) return;
        lv_obj_t* lbl = lv_label_create(_area);
        lv_label_set_text(lbl, msg);
        lv_obj_set_style_text_color(lbl, gTheme->errorText, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontNormal, LV_PART_MAIN);
        lv_obj_center(lbl);
    }

    void _onHud(const char* l, const char* r) {
        if (_hudLeft) lv_label_set_text(_hudLeft, l ? l : "");
        if (_hudRight) lv_label_set_text(_hudRight, r ? r : "");
    }

    void _onGameOver(int score) {
        _modal.show(_screen, _id.c_str(), score, [this]() {
            // Restart cleanly by replacing the current screen with a fresh
            // instance. _id is copied into the new screen before `this` is
            // destroyed by replaceCurrent().
            ScreenManager::getInstance().replaceCurrent(new LuaGameScreen(_id));
        });
    }
};
