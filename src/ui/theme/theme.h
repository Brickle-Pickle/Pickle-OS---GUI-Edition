#pragma once
#include <lvgl.h>
#include <Preferences.h>

struct Theme {
    lv_color_t primary;
    lv_color_t primaryLight;
    lv_color_t primaryDark;
    lv_color_t secondary;
    lv_color_t secondaryLight;
    lv_color_t secondaryDark;
    lv_color_t background;
    lv_color_t backgroundPopup;
    lv_color_t textDark;
    lv_color_t textSoft;
    lv_color_t successText;
    lv_color_t successBg;
    lv_color_t errorText;
    lv_color_t errorBg;
    lv_color_t alertText;
    lv_color_t alertBg;
};

inline const Theme THEME_LIGHT = {
    .primary = lv_color_hex(0x2D7DD2),
    .primaryLight = lv_color_hex(0x6AAEE8),
    .primaryDark = lv_color_hex(0x1A4F8A),
    .secondary = lv_color_hex(0x3DAA74),
    .secondaryLight = lv_color_hex(0x72C99A),
    .secondaryDark = lv_color_hex(0x1F6644),
    .background = lv_color_hex(0xF4F2ED),
    .backgroundPopup= lv_color_hex(0xFFFFFF),
    .textDark = lv_color_hex(0x1C1C1A),
    .textSoft = lv_color_hex(0x5A5955),
    .successText = lv_color_hex(0x1F6644),
    .successBg = lv_color_hex(0xD6F0E3),
    .errorText = lv_color_hex(0x8C1F1F),
    .errorBg = lv_color_hex(0xFADDDD),
    .alertText = lv_color_hex(0x7A4A08),
    .alertBg = lv_color_hex(0xFAEACB),
};

inline const Theme THEME_DARK = {
    .primary = lv_color_hex(0x5BA3E8),
    .primaryLight = lv_color_hex(0x8FC3F4),
    .primaryDark = lv_color_hex(0x2D6FAF),
    .secondary = lv_color_hex(0x50C48A),
    .secondaryLight = lv_color_hex(0x86D9AA),
    .secondaryDark = lv_color_hex(0x26795A),
    .background = lv_color_hex(0x161614),
    .backgroundPopup = lv_color_hex(0x242422),
    .textDark = lv_color_hex(0xE8E6DF),
    .textSoft = lv_color_hex(0x9C9A93),
    .successText = lv_color_hex(0x7EE0A8),
    .successBg = lv_color_hex(0x0D3D26),
    .errorText = lv_color_hex(0xF4A0A0),
    .errorBg = lv_color_hex(0x3D1212),
    .alertText = lv_color_hex(0xF2C471),
    .alertBg = lv_color_hex(0x3D2606),
};

// Active theme pointer, set once at boot
inline const Theme* gTheme = &THEME_DARK;

inline void setTheme(bool dark) {
    gTheme = dark ? &THEME_DARK : &THEME_LIGHT;
}

inline void saveTheme(bool dark) {
    gTheme = dark ? &THEME_DARK : &THEME_LIGHT;
    Preferences prefs;
    prefs.begin("pickle-os", false);
    prefs.putBool("darkTheme", dark);
    prefs.end();
}

inline void loadTheme() {
    Preferences prefs;
    prefs.begin("pickle-os", true);
    bool dark = prefs.getBool("darkTheme", true);
    prefs.end();
    gTheme = dark ? &THEME_DARK : &THEME_LIGHT;
}

inline bool isDarkTheme() {
    return gTheme == &THEME_DARK;
}