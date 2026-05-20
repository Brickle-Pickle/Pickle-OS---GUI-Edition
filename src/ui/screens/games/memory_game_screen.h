#pragma once
#include <lvgl.h>
#include <vector>
#include "../screen_base.h"
#include "../../screen_manager.h"
#include "../../theme/theme.h"
#include "game_over_modal.h"

// Memory - a Simon-style memory game with four colored tiles.
// Each round appends a random tile to the sequence, which is replayed
// for the player. The player must repeat it tile by tile. A mistake
// ends the game; the score is the length of the longest sequence
// completed correctly.
class MemoryGameScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildTiles();
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
    static constexpr int TILE_SIZE = 100;
    static constexpr int TILE_GAP  = 8;

    // Bright/dark color pairs for the four tiles (red, green, blue, yellow)
    static lv_color_t _bright(int i) {
        static const uint32_t hex[4] = { 0xE53935, 0x43A047, 0x1E88E5, 0xFDD835 };
        return lv_color_hex(hex[i]);
    }
    static lv_color_t _dark(int i) {
        static const uint32_t hex[4] = { 0x6E1A18, 0x1F4A24, 0x0D3C66, 0x7A6818 };
        return lv_color_hex(hex[i]);
    }

    lv_obj_t*   _scoreLbl   = nullptr;
    lv_obj_t*   _statusLbl  = nullptr;
    lv_obj_t*   _tiles[4]   = { nullptr, nullptr, nullptr, nullptr };
    lv_timer_t* _stepTimer  = nullptr;
    lv_timer_t* _unflashTmr = nullptr;
    std::vector<int> _sequence;
    int  _playIdx   = 0;
    int  _inputIdx  = 0;
    int  _flashedTile = -1;
    bool _showing  = false;
    bool _gameOver = false;
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
        lv_label_set_text(_scoreLbl, "Level: 0");
        lv_obj_set_style_text_color(_scoreLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_scoreLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_scoreLbl, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildTiles() {
        int gw = TILE_SIZE * 2 + TILE_GAP;
        int x0 = (240 - gw) / 2;
        int y0 = 56;
        for (int i = 0; i < 4; i++) {
            int col = i % 2;
            int row = i / 2;
            int x = x0 + col * (TILE_SIZE + TILE_GAP);
            int y = y0 + row * (TILE_SIZE + TILE_GAP);

            lv_obj_t* b = lv_btn_create(_screen);
            lv_obj_set_size(b, TILE_SIZE, TILE_SIZE);
            lv_obj_set_pos(b, x, y);
            lv_obj_set_style_bg_color(b, _dark(i), LV_PART_MAIN);
            lv_obj_set_style_bg_color(b, _bright(i), LV_STATE_PRESSED);
            lv_obj_set_style_radius(b, 12, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_user_data(b, (void*)(intptr_t)i);
            lv_obj_add_event_cb(b, [](lv_event_t* e) {
                MemoryGameScreen* s = (MemoryGameScreen*)lv_event_get_user_data(e);
                int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                s->_onTilePressed(idx);
            }, LV_EVENT_CLICKED, this);
            _tiles[i] = b;
        }

        _statusLbl = lv_label_create(_screen);
        lv_label_set_text(_statusLbl, "Watch...");
        lv_obj_set_style_text_color(_statusLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_statusLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_statusLbl, LV_ALIGN_BOTTOM_MID, 0, -10);
    }

    void _startGame() {
        _stopTimers();
        _sequence.clear();
        _playIdx  = 0;
        _inputIdx = 0;
        _gameOver = false;
        _flashedTile = -1;
        for (int i = 0; i < 4; i++) {
            lv_obj_set_style_bg_color(_tiles[i], _dark(i), LV_PART_MAIN);
        }
        lv_label_set_text(_scoreLbl, "Level: 0");
        _newRound();
    }

    void _stopTimers() {
        if (_stepTimer)  { lv_timer_del(_stepTimer);  _stepTimer  = nullptr; }
        if (_unflashTmr) { lv_timer_del(_unflashTmr); _unflashTmr = nullptr; }
    }

    void _newRound() {
        _sequence.push_back(random(0, 4));
        _playIdx  = 0;
        _inputIdx = 0;
        _showing  = true;
        lv_label_set_text_fmt(_scoreLbl, "Level: %d", (int)_sequence.size());
        lv_label_set_text(_statusLbl, "Watch...");
        _scheduleStep(700);
    }

    void _scheduleStep(int delay) {
        if (_stepTimer) { lv_timer_del(_stepTimer); _stepTimer = nullptr; }
        _stepTimer = lv_timer_create([](lv_timer_t* t) {
            ((MemoryGameScreen*)t->user_data)->_playStep();
        }, delay, this);
        lv_timer_set_repeat_count(_stepTimer, 1);
    }

    void _playStep() {
        if (_stepTimer) { lv_timer_del(_stepTimer); _stepTimer = nullptr; }
        if (_gameOver) return;
        if (_playIdx >= (int)_sequence.size()) {
            _showing = false;
            lv_label_set_text(_statusLbl, "Your turn");
            return;
        }
        _flash(_sequence[_playIdx]);
        _playIdx++;
        _scheduleStep(550);
    }

    void _flash(int idx) {
        if (idx < 0 || idx >= 4) return;
        _flashedTile = idx;
        lv_obj_set_style_bg_color(_tiles[idx], _bright(idx), LV_PART_MAIN);
        if (_unflashTmr) { lv_timer_del(_unflashTmr); _unflashTmr = nullptr; }
        _unflashTmr = lv_timer_create([](lv_timer_t* t) {
            MemoryGameScreen* s = (MemoryGameScreen*)t->user_data;
            if (s->_flashedTile >= 0) {
                lv_obj_set_style_bg_color(s->_tiles[s->_flashedTile],
                                          _dark(s->_flashedTile), LV_PART_MAIN);
                s->_flashedTile = -1;
            }
            lv_timer_del(s->_unflashTmr);
            s->_unflashTmr = nullptr;
        }, 320, this);
        lv_timer_set_repeat_count(_unflashTmr, 1);
    }

    void _onTilePressed(int idx) {
        if (_gameOver || _showing) return;
        _flash(idx);

        int expected = _sequence[_inputIdx];
        if (idx != expected) {
            _endGame();
            return;
        }
        _inputIdx++;
        if (_inputIdx >= (int)_sequence.size()) {
            // Round complete - schedule the next sequence after a short pause
            _showing = true;
            lv_label_set_text(_statusLbl, "Nice!");
            if (_stepTimer) { lv_timer_del(_stepTimer); _stepTimer = nullptr; }
            _stepTimer = lv_timer_create([](lv_timer_t* t) {
                MemoryGameScreen* s = (MemoryGameScreen*)t->user_data;
                lv_timer_del(s->_stepTimer);
                s->_stepTimer = nullptr;
                if (!s->_gameOver) s->_newRound();
            }, 700, this);
            lv_timer_set_repeat_count(_stepTimer, 1);
        }
    }

    void _endGame() {
        _gameOver = true;
        _stopTimers();
        // Score is the last fully reproduced sequence length
        int score = (int)_sequence.size() - 1;
        if (score < 0) score = 0;
        _modal.show(_screen, "memory", score, [this]() {
            _startGame();
        });
    }
};
