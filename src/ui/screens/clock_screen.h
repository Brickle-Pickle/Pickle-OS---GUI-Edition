#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../../network/wifi_manager.h"
#include <time.h>

// Clock & Calendar app - large digital clock with full month calendar view
class ClockScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildClock();
        _buildCalendar();

        _refresh();
        _tickTimer = lv_timer_create([](lv_timer_t* t) {
            ((ClockScreen*)t->user_data)->_refresh();
        }, 1000, this);
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
    lv_obj_t* _timeLabel = nullptr;
    lv_obj_t* _dateLabel = nullptr;
    lv_obj_t* _calendar  = nullptr;
    lv_timer_t* _tickTimer = nullptr;
    int _shownMonth = -1;
    int _shownYear = -1;
    int _shownDay = -1;

    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, 240, 36);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Exit button (left)
        lv_obj_t* exitBtn = lv_btn_create(header);
        lv_obj_set_size(exitBtn, 56, 28);
        lv_obj_align(exitBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(exitBtn, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(exitBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(exitBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(exitBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(exitBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* exitLbl = lv_label_create(exitBtn);
        lv_label_set_text(exitLbl, "Exit");
        lv_obj_set_style_text_color(exitLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(exitLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(exitLbl);
        lv_obj_add_event_cb(exitBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        // Title
        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Clock");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildClock() {
        // Big time
        _timeLabel = lv_label_create(_screen);
        lv_label_set_text(_timeLabel, "--:--:--");
        lv_obj_set_style_text_color(_timeLabel, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_timeLabel, gFontLarge, LV_PART_MAIN);
        lv_obj_align(_timeLabel, LV_ALIGN_TOP_MID, 0, 46);

        // Date subtitle
        _dateLabel = lv_label_create(_screen);
        lv_label_set_text(_dateLabel, "Waiting for NTP...");
        lv_obj_set_style_text_color(_dateLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_dateLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_dateLabel, LV_ALIGN_TOP_MID, 0, 74);
    }

    void _buildCalendar() {
        _calendar = lv_calendar_create(_screen);
        lv_obj_set_size(_calendar, 230, 200);
        lv_obj_align(_calendar, LV_ALIGN_TOP_MID, 0, 96);
        lv_obj_set_style_bg_color(_calendar, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(_calendar, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_calendar, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_color(_calendar, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_border_width(_calendar, 1, LV_PART_MAIN);
        lv_obj_set_style_radius(_calendar, 8, LV_PART_MAIN);

        // Highlight "today" cell once we know the date
        lv_obj_set_style_bg_color(_calendar, gTheme->primary, LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(_calendar, gTheme->textDark, LV_PART_ITEMS | LV_STATE_FOCUSED);
    }

    void _refresh() {
        time_t now = time(nullptr);
        if (now < 1700000000) {
            // NTP not synced yet
            lv_label_set_text(_timeLabel, "--:--:--");
            lv_label_set_text(_dateLabel, "Waiting for NTP...");
            return;
        }

        struct tm t;
        localtime_r(&now, &t);

        // HH:MM:SS
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        const char* cur = lv_label_get_text(_timeLabel);
        if (!cur || strcmp(cur, buf) != 0) {
            lv_label_set_text(_timeLabel, buf);
        }

        // Date line: "Wed, 20 May 2026"
        static const char* WD[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* MO[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
        char dbuf[32];
        snprintf(dbuf, sizeof(dbuf), "%s, %d %s %d",
                 WD[t.tm_wday], t.tm_mday, MO[t.tm_mon], t.tm_year + 1900);
        const char* dcur = lv_label_get_text(_dateLabel);
        if (!dcur || strcmp(dcur, dbuf) != 0) {
            lv_label_set_text(_dateLabel, dbuf);
        }

        // Update calendar shown month + today highlight only when day changes
        int year  = t.tm_year + 1900;
        int month = t.tm_mon + 1;
        int day   = t.tm_mday;
        if (year != _shownYear || month != _shownMonth || day != _shownDay) {
            lv_calendar_set_today_date(_calendar, year, month, day);
            lv_calendar_set_showed_date(_calendar, year, month);
            lv_calendar_date_t hl = { (uint16_t)year, (uint8_t)month, (uint8_t)day };
            lv_calendar_set_highlighted_dates(_calendar, &hl, 1);
            _shownYear  = year;
            _shownMonth = month;
            _shownDay   = day;
        }
    }
};
