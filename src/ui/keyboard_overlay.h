#pragma once
#include <lvgl.h>

// Reusable keyboard overlay — attach to any screen's lv_obj
class KeyboardOverlay {
public:
    // Call once per screen that needs a keyboard, pass the screen's root lv_obj
    void create(lv_obj_t* parent) {
        _kb = lv_keyboard_create(parent);
        lv_obj_set_size(_kb, 240, 140);
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        hide(); // start hidden
    }

    void linkTo(lv_obj_t* textarea) {
        lv_keyboard_set_textarea(_kb, textarea);
    }

    void show(lv_keyboard_mode_t mode = LV_KEYBOARD_MODE_TEXT_LOWER) {
        lv_keyboard_set_mode(_kb, mode);
        lv_obj_clear_flag(_kb, LV_OBJ_FLAG_HIDDEN);
    }

    void showNumeric() { show(LV_KEYBOARD_MODE_NUMBER); }

    void hide() { lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN); }

    lv_obj_t* obj() { return _kb; }

private:
    lv_obj_t* _kb = nullptr;
};
