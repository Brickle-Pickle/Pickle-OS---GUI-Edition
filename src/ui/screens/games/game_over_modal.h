#pragma once
#include <lvgl.h>
#include <functional>
#include "leaderboard.h"
#include "../../theme/theme.h"
#include "../../screen_manager.h"
#include "../../toast/toast_manager.h"

// Reusable Game Over overlay - shows score, asks for player name (max 10),
// saves to leaderboard, then displays the top scores with Retry/Back actions.
class GameOverModal {
public:
    using PlayAgainCb = std::function<void()>;

    // Build the overlay over the given parent (typically the screen root).
    // gameId is the file basename used by Leaderboard (e.g. "tap" or "memory").
    void show(lv_obj_t* parent, const char* gameId, int score, PlayAgainCb playAgain) {
        _gameId    = gameId;
        _score     = score;
        _playAgain = playAgain;

        // Backdrop covering the whole screen
        _bg = lv_obj_create(parent);
        lv_obj_set_size(_bg, 240, 320);
        lv_obj_set_pos(_bg, 0, 0);
        lv_obj_set_style_bg_color(_bg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_bg, LV_OPA_70, LV_PART_MAIN);
        lv_obj_set_style_border_width(_bg, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_bg, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_bg, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_bg, LV_OBJ_FLAG_SCROLLABLE);

        _buildEntryCard();
    }

    // Destroy all LVGL objects owned by the modal.
    void dismiss() {
        if (_kb)   { lv_obj_del(_kb);   _kb   = nullptr; }
        if (_card) { lv_obj_del(_card); _card = nullptr; }
        if (_bg)   { lv_obj_del(_bg);   _bg   = nullptr; }
        _input = nullptr;
    }

private:
    lv_obj_t*   _bg    = nullptr;
    lv_obj_t*   _card  = nullptr;
    lv_obj_t*   _input = nullptr;
    lv_obj_t*   _kb    = nullptr;
    const char* _gameId = "";
    int         _score  = 0;
    PlayAgainCb _playAgain;

    void _buildEntryCard() {
        _card = lv_obj_create(_bg);
        lv_obj_set_size(_card, 220, 170);
        lv_obj_align(_card, LV_ALIGN_TOP_MID, 0, 10);
        lv_obj_set_style_bg_color(_card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(_card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(_card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(_card);
        lv_label_set_text(title, "Game Over");
        lv_obj_set_style_text_color(title, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t* scoreLbl = lv_label_create(_card);
        lv_label_set_text_fmt(scoreLbl, "Score: %d", _score);
        lv_obj_set_style_text_color(scoreLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(scoreLbl, gFontNormal, LV_PART_MAIN);
        lv_obj_align(scoreLbl, LV_ALIGN_TOP_MID, 0, 22);

        lv_obj_t* hint = lv_label_create(_card);
        lv_label_set_text(hint, "Name (max 10):");
        lv_obj_set_style_text_color(hint, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, gFontSmall, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 52);

        _input = lv_textarea_create(_card);
        lv_obj_set_size(_input, 200, 32);
        lv_obj_align(_input, LV_ALIGN_TOP_LEFT, 0, 70);
        lv_textarea_set_one_line(_input, true);
        lv_textarea_set_max_length(_input, 10);
        lv_textarea_set_placeholder_text(_input, "Player");
        lv_obj_set_style_bg_color(_input, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_text_color(_input, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_input, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_color(_input, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_border_width(_input, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_input, 6, LV_PART_MAIN);

        // Save button
        lv_obj_t* saveBtn = lv_btn_create(_card);
        lv_obj_set_size(saveBtn, 90, 32);
        lv_obj_align(saveBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(saveBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(saveBtn, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(saveBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(saveBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(saveBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* sLbl = lv_label_create(saveBtn);
        lv_label_set_text(sLbl, "Save");
        lv_obj_set_style_text_color(sLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(sLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(sLbl);
        lv_obj_add_event_cb(saveBtn, [](lv_event_t* e) {
            ((GameOverModal*)lv_event_get_user_data(e))->_onSave();
        }, LV_EVENT_CLICKED, this);

        // Skip button - go straight to leaderboard without saving
        lv_obj_t* skipBtn = lv_btn_create(_card);
        lv_obj_set_size(skipBtn, 90, 32);
        lv_obj_align(skipBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(skipBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(skipBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(skipBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(skipBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* skLbl = lv_label_create(skipBtn);
        lv_label_set_text(skLbl, "Skip");
        lv_obj_set_style_text_color(skLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(skLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(skLbl);
        lv_obj_add_event_cb(skipBtn, [](lv_event_t* e) {
            ((GameOverModal*)lv_event_get_user_data(e))->_showLeaderboard();
        }, LV_EVENT_CLICKED, this);

        // On-screen keyboard wired to the input
        _kb = lv_keyboard_create(_bg);
        lv_obj_set_size(_kb, 240, 130);
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_textarea(_kb, _input);
        lv_keyboard_set_mode(_kb, LV_KEYBOARD_MODE_TEXT_UPPER);
    }

    void _onSave() {
        String name = _input ? String(lv_textarea_get_text(_input)) : String("");
        name.trim();
        if (name.length() == 0) name = "Player";
        if (Leaderboard::save(_gameId, name, _score)) {
            ToastManager::getInstance().showToast("Saved!", ToastType::SUCCESS);
        } else {
            ToastManager::getInstance().showToast("Save failed", ToastType::ALERT);
        }
        _showLeaderboard();
    }

    void _showLeaderboard() {
        if (_kb)   { lv_obj_del(_kb);   _kb   = nullptr; }
        if (_card) { lv_obj_del(_card); _card = nullptr; }
        _input = nullptr;

        lv_obj_t* card = lv_obj_create(_bg);
        lv_obj_set_size(card, 220, 260);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        _card = card;

        lv_obj_t* title = lv_label_create(card);
        lv_label_set_text(title, "Leaderboard");
        lv_obj_set_style_text_color(title, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

        auto entries = Leaderboard::load(_gameId, 5);
        int y = 32;
        if (entries.empty()) {
            lv_obj_t* none = lv_label_create(card);
            lv_label_set_text(none, "No scores yet");
            lv_obj_set_style_text_color(none, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(none, gFontSmall, LV_PART_MAIN);
            lv_obj_align(none, LV_ALIGN_TOP_MID, 0, y);
        } else {
            int i = 1;
            for (auto& e : entries) {
                lv_obj_t* row = lv_label_create(card);
                lv_label_set_text_fmt(row, "%d. %-10s %4d", i, e.name.c_str(), e.score);
                lv_obj_set_style_text_color(row, gTheme->textDark, LV_PART_MAIN);
                lv_obj_set_style_text_font(row, gFontSmall, LV_PART_MAIN);
                lv_obj_align(row, LV_ALIGN_TOP_LEFT, 4, y);
                y += 18;
                i++;
            }
        }

        // Retry button - replays the current game
        lv_obj_t* againBtn = lv_btn_create(card);
        lv_obj_set_size(againBtn, 90, 32);
        lv_obj_align(againBtn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(againBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(againBtn, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(againBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(againBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(againBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* aLbl = lv_label_create(againBtn);
        lv_label_set_text(aLbl, "Retry");
        lv_obj_set_style_text_color(aLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(aLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(aLbl);
        lv_obj_add_event_cb(againBtn, [](lv_event_t* e) {
            GameOverModal* m = (GameOverModal*)lv_event_get_user_data(e);
            auto cb = m->_playAgain;
            m->dismiss();
            if (cb) cb();
        }, LV_EVENT_CLICKED, this);

        // Back button - pops to the games screen
        lv_obj_t* backBtn = lv_btn_create(card);
        lv_obj_set_size(backBtn, 90, 32);
        lv_obj_align(backBtn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(backBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(backBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* bLbl = lv_label_create(backBtn);
        lv_label_set_text(bLbl, "Back");
        lv_obj_set_style_text_color(bLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(bLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(bLbl);
        lv_obj_add_event_cb(backBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);
    }
};
