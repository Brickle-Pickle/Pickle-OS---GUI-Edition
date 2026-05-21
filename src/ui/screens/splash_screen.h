#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "home_screen.h"
#include "pin_lock_screen.h"

#define SPLASH_DURATION_MS 2500

class SplashScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildLogo();
        _buildTitle();
        _buildSubtitle();

        // Pass `this` so the callback can null _timer before LVGL auto-deletes it
        _timer = lv_timer_create(_onTimeout, SPLASH_DURATION_MS, this);
        lv_timer_set_repeat_count(_timer, 1);
    }

    void onDestroy() override {
        // _timer may already be null if the timeout fired (LVGL auto-deletes repeat_count=1)
        if (_timer) {
            lv_timer_del(_timer);
            _timer = nullptr;
        }
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_timer_t* _timer = nullptr;

    void _buildLogo() {
        extern const lv_img_dsc_t img_splash_logo;
        lv_obj_t* img = lv_img_create(_screen);
        lv_img_set_src(img, &img_splash_logo);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -30);
    }

    void _buildTitle() {
        lv_obj_t* lbl = lv_label_create(_screen);
        lv_label_set_text(lbl, "Pickle OS");
        lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontLarge, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 80);
    }

    void _buildSubtitle() {
        lv_obj_t* lbl = lv_label_create(_screen);
        lv_label_set_text(lbl, "GUI Edition");
        lv_obj_set_style_text_color(lbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 110);
    }

    static void _onTimeout(lv_timer_t* timer) {
        SplashScreen* self = static_cast<SplashScreen*>(timer->user_data);
        // Null the pointer NOW: LVGL will auto-delete the timer when this callback returns
        // (repeat_count=1). If we don't null it, onDestroy() called below would double-free it.
        self->_timer = nullptr;
        // replaceCurrent removes the splash from the nav stack so goBack() can't return to it.
        // If the user has configured a device PIN, gate access through the lock screen.
        ScreenBase* next = PinLockScreen::hasPin()
            ? (ScreenBase*)new PinLockScreen()
            : (ScreenBase*)new HomeScreen();
        ScreenManager::getInstance().replaceCurrent(next);
    }
};
