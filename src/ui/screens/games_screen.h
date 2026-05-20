#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "games/tap_game_screen.h"
#include "games/memory_game_screen.h"

// GamesScreen - Lists the games installed on the device and exposes a
// (placeholder) entry point to the future game store.
class GamesScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildContent();
        _buildStoreButton();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    void _buildHeader() {
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

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Games");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildContent() {
        struct GameEntry {
            const char*   icon;
            const char*   name;
            const char*   desc;
            lv_event_cb_t cb;
        };

        static auto cbTap = [](lv_event_t*) {
            ScreenManager::getInstance().navigateTo(
                new TapGameScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
        };
        static auto cbMem = [](lv_event_t*) {
            ScreenManager::getInstance().navigateTo(
                new MemoryGameScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
        };

        const GameEntry games[] = {
            { LV_SYMBOL_PLAY,    "Tap Frenzy", "Tap targets before they vanish",  cbTap },
            { LV_SYMBOL_REFRESH, "Memory",     "Repeat the color sequence",       cbMem },
        };
        const int N = sizeof(games) / sizeof(games[0]);

        int y = 48;
        for (int i = 0; i < N; i++) {
            lv_obj_t* card = lv_btn_create(_screen);
            lv_obj_set_size(card, 220, 64);
            lv_obj_set_pos(card, 10, y);
            lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
            lv_obj_set_style_bg_color(card, gTheme->primaryDark, LV_STATE_PRESSED);
            lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* ico = lv_label_create(card);
            lv_label_set_text(ico, games[i].icon);
            lv_obj_set_style_text_color(ico, gTheme->primary, LV_PART_MAIN);
            lv_obj_set_style_text_font(ico, gFontIconLarge, LV_PART_MAIN);
            lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

            lv_obj_t* nm = lv_label_create(card);
            lv_label_set_text(nm, games[i].name);
            lv_obj_set_style_text_color(nm, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(nm, gFontNormal, LV_PART_MAIN);
            lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 36, 0);

            lv_obj_t* d = lv_label_create(card);
            lv_label_set_text(d, games[i].desc);
            lv_obj_set_style_text_color(d, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(d, gFontSmall, LV_PART_MAIN);
            lv_obj_align(d, LV_ALIGN_BOTTOM_LEFT, 36, 0);

            lv_obj_add_event_cb(card, games[i].cb, LV_EVENT_CLICKED, nullptr);
            y += 72;
        }
    }

    // Store button pinned to the bottom - not implemented yet.
    void _buildStoreButton() {
        lv_obj_t* store = lv_btn_create(_screen);
        lv_obj_set_size(store, 220, 44);
        lv_obj_set_pos(store, 10, 320 - 56);
        lv_obj_set_style_bg_color(store, gTheme->secondary, LV_PART_MAIN);
        lv_obj_set_style_bg_color(store, gTheme->secondaryDark, LV_STATE_PRESSED);
        lv_obj_set_style_radius(store, 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(store, 0, LV_PART_MAIN);
        lv_obj_clear_flag(store, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(store);
        lv_label_set_text(lbl, LV_SYMBOL_DOWNLOAD "  Get more games");
        lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontNormal, LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(store, [](lv_event_t*) {
            ToastManager::getInstance().showToast("Store coming soon!", ToastType::ALERT);
        }, LV_EVENT_CLICKED, nullptr);
    }
};
