#pragma once
#include <lvgl.h>
#include "../screen_base.h"
#include "../../screen_manager.h"
#include "../../theme/theme.h"
#include "game_over_modal.h"

// Tap Frenzy - a single circular target appears at random positions.
// Tap it before its lifetime expires to score. The lifetime shortens with
// the player's score to make it progressively harder. Game ends after 30s.
class TapGameScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildArea();
        _startGame();
    }

    void onDestroy() override {
        _stopTimers();
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    static constexpr int GAME_TIME = 30; // seconds
    static constexpr int TARGET_SIZE = 56;

    lv_obj_t*   _scoreLbl   = nullptr;
    lv_obj_t*   _timeLbl    = nullptr;
    lv_obj_t*   _area       = nullptr;
    lv_obj_t*   _target     = nullptr;
    lv_timer_t* _tickTimer  = nullptr;
    lv_timer_t* _expireTmr  = nullptr;
    int         _score      = 0;
    int         _timeLeft   = GAME_TIME;
    bool        _gameOver   = false;
    GameOverModal _modal;

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

        _scoreLbl = lv_label_create(header);
        lv_label_set_text(_scoreLbl, "Score: 0");
        lv_obj_set_style_text_color(_scoreLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_scoreLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_scoreLbl, LV_ALIGN_CENTER, 0, 0);

        _timeLbl = lv_label_create(header);
        lv_label_set_text_fmt(_timeLbl, "%ds", GAME_TIME);
        lv_obj_set_style_text_color(_timeLbl, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_timeLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_timeLbl, LV_ALIGN_RIGHT_MID, -8, 0);
    }

    void _buildArea() {
        _area = lv_obj_create(_screen);
        lv_obj_set_size(_area, 240, 320 - 36);
        lv_obj_set_pos(_area, 0, 36);
        lv_obj_set_style_bg_color(_area, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_area, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_area, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_area, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_area, LV_OBJ_FLAG_SCROLLABLE);
    }

    void _startGame() {
        _score = 0;
        _timeLeft = GAME_TIME;
        _gameOver = false;
        _updateHud();
        _spawnTarget();
        _tickTimer = lv_timer_create([](lv_timer_t* t) {
            ((TapGameScreen*)t->user_data)->_onTick();
        }, 1000, this);
    }

    void _stopTimers() {
        if (_tickTimer) { lv_timer_del(_tickTimer); _tickTimer = nullptr; }
        if (_expireTmr) { lv_timer_del(_expireTmr); _expireTmr = nullptr; }
    }

    void _updateHud() {
        lv_label_set_text_fmt(_scoreLbl, "Score: %d", _score);
        lv_label_set_text_fmt(_timeLbl, "%ds", _timeLeft);
    }

    void _onTick() {
        if (_gameOver) return;
        _timeLeft--;
        _updateHud();
        if (_timeLeft <= 0) _endGame();
    }

    void _spawnTarget() {
        if (_gameOver) return;
        if (_target) { lv_obj_del(_target); _target = nullptr; }

        int areaH = 320 - 36;
        int x = random(8, 240 - TARGET_SIZE - 8);
        int y = random(8, areaH - TARGET_SIZE - 8);

        _target = lv_btn_create(_area);
        lv_obj_set_size(_target, TARGET_SIZE, TARGET_SIZE);
        lv_obj_set_pos(_target, x, y);
        lv_obj_set_style_bg_color(_target, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_target, gTheme->primaryLight, LV_STATE_PRESSED);
        lv_obj_set_style_radius(_target, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(_target, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_target, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(_target, [](lv_event_t* e) {
            TapGameScreen* s = (TapGameScreen*)lv_event_get_user_data(e);
            if (s->_gameOver) return;
            s->_score++;
            s->_updateHud();
            s->_spawnTarget();
        }, LV_EVENT_CLICKED, this);

        // Lifetime shrinks as the player scores higher (clamped to 500ms)
        int life = 1300 - _score * 25;
        if (life < 500) life = 500;
        if (_expireTmr) { lv_timer_del(_expireTmr); _expireTmr = nullptr; }
        _expireTmr = lv_timer_create([](lv_timer_t* t) {
            TapGameScreen* s = (TapGameScreen*)t->user_data;
            lv_timer_del(s->_expireTmr);
            s->_expireTmr = nullptr;
            if (!s->_gameOver) s->_spawnTarget();
        }, life, this);
        lv_timer_set_repeat_count(_expireTmr, 1);
    }

    void _endGame() {
        _gameOver = true;
        _stopTimers();
        if (_target) { lv_obj_del(_target); _target = nullptr; }
        _modal.show(_screen, "tap", _score, [this]() {
            _startGame();
        });
    }
};
