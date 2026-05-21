#pragma once
#include <Preferences.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../keyboard_overlay.h"
#include "../theme/theme.h"
#include "home_screen.h"

// PinLockScreen - prompts the user for the device PIN before entering the OS.
// The PIN is stored in NVS (internal flash) under namespace "pickle-os", key "pin".
// There is no attempt limit by design: a wrong PIN just clears the field.
class PinLockScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* icon = lv_label_create(_screen);
        lv_label_set_text(icon, LV_SYMBOL_KEYBOARD);
        lv_obj_set_style_text_color(icon, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(icon, gFontIconLarge, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 24);

        lv_obj_t* title = lv_label_create(_screen);
        lv_label_set_text(title, "Enter PIN");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontLarge, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

        _hint = lv_label_create(_screen);
        lv_label_set_text(_hint, "Type your PIN and press OK");
        lv_obj_set_style_text_color(_hint, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_hint, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_hint, LV_ALIGN_TOP_MID, 0, 100);

        _ta = lv_textarea_create(_screen);
        lv_obj_set_size(_ta, 200, 44);
        lv_obj_align(_ta, LV_ALIGN_TOP_MID, 0, 130);
        lv_textarea_set_one_line(_ta, true);
        lv_textarea_set_password_mode(_ta, true);
        lv_textarea_set_max_length(_ta, 16);
        lv_textarea_set_placeholder_text(_ta, "PIN");
        lv_obj_set_style_bg_color(_ta, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_text_color(_ta, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_border_color(_ta, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_border_width(_ta, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_ta, 8, LV_PART_MAIN);
        lv_obj_set_style_text_align(_ta, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        _kb.create(_screen);
        _kb.linkTo(_ta);
        _kb.showNumeric();

        lv_obj_add_event_cb(_kb.obj(), _onReady, LV_EVENT_READY, this);
        // Cancel does nothing: PIN entry cannot be dismissed.
        lv_obj_add_event_cb(_kb.obj(), _onCancel, LV_EVENT_CANCEL, this);
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

    // Reads the stored PIN from NVS. Returns empty string when no PIN is set.
    static String storedPin() {
        Preferences prefs;
        prefs.begin("pickle-os", true);
        String pin = prefs.getString("pin", "");
        prefs.end();
        return pin;
    }

    static void savePin(const String& pin) {
        Preferences prefs;
        prefs.begin("pickle-os", false);
        prefs.putString("pin", pin);
        prefs.end();
    }

    static void clearPin() {
        Preferences prefs;
        prefs.begin("pickle-os", false);
        prefs.remove("pin");
        prefs.end();
    }

    static bool hasPin() { return storedPin().length() > 0; }

private:
    KeyboardOverlay _kb;
    lv_obj_t* _ta = nullptr;
    lv_obj_t* _hint = nullptr;

    static void _onReady(lv_event_t* e) {
        PinLockScreen* self = (PinLockScreen*)lv_event_get_user_data(e);
        String entered = String(lv_textarea_get_text(self->_ta));
        String stored = storedPin();
        if (entered == stored && stored.length() > 0) {
            ScreenManager::getInstance().replaceCurrent(new HomeScreen());
        } else {
            lv_textarea_set_text(self->_ta, "");
            lv_label_set_text(self->_hint, "Wrong PIN, try again");
            lv_obj_set_style_text_color(self->_hint, gTheme->errorText, LV_PART_MAIN);
        }
    }

    static void _onCancel(lv_event_t* e) {
        PinLockScreen* self = (PinLockScreen*)lv_event_get_user_data(e);
        lv_textarea_set_text(self->_ta, "");
    }
};
