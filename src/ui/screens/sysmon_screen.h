#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../../network/wifi_manager.h"
#include "../../storage/sd_manager.h"
#include "../../system/sysmon_history.h"

// SysMonScreen - Real-time ESP32 system monitor.
// Shows heap usage with a live chart, CPU frequency, uptime, WiFi signal,
// SD card space, internal die temperature, and FreeRTOS task count.
// All values refresh every second via an lv_timer on the LVGL task.
class SysMonScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildScrollArea();
        _buildHeapChart();
        _buildStatGrid();
        _buildSdBar();
        _buildBottomRow();

        // First paint before the timer fires
        _refresh();

        _timer = lv_timer_create([](lv_timer_t* t) {
            ((SysMonScreen*)t->user_data)->_refresh();
        }, 1000, this);
    }

    void onDestroy() override {
        if (_timer) { lv_timer_del(_timer); _timer = nullptr; }
        if (_screen) { lv_obj_del(_screen); _screen = nullptr; }
    }

private:
    // Metrics displayed
    static constexpr int CHART_POINTS = 60;

    lv_obj_t* _scroll = nullptr;
    lv_obj_t* _chart = nullptr;
    lv_chart_series_t* _heapSeries = nullptr;
    lv_obj_t* _heapLbl = nullptr;
    lv_obj_t* _heapPctLbl = nullptr;
    lv_obj_t* _cpuLbl = nullptr;
    lv_obj_t* _uptimeLbl = nullptr;
    lv_obj_t* _wifiLbl = nullptr;
    lv_obj_t* _ipLbl = nullptr;
    lv_obj_t* _sdBar = nullptr;
    lv_obj_t* _sdLbl = nullptr;
    lv_obj_t* _tempLbl = nullptr;
    lv_obj_t* _tasksLbl = nullptr;
    lv_timer_t* _timer = nullptr;

    static constexpr int W = 240;
    static constexpr int H = 320;
    static constexpr int HEADER_H = 32;
    static constexpr int CONTENT_H = H - HEADER_H;

    void _buildHeader() {
        lv_obj_t* hdr = lv_obj_create(_screen);
        lv_obj_set_size(hdr, W, HEADER_H);
        lv_obj_set_pos(hdr, 0, 0);
        lv_obj_set_style_bg_color(hdr, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(hdr, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(hdr, 0, LV_PART_MAIN);
        lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* back = lv_btn_create(hdr);
        lv_obj_set_size(back, 32, 26);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(back, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(back, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
        lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* backIco = lv_label_create(back);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(backIco, gFontIcon, LV_PART_MAIN);
        lv_obj_center(backIco);
        lv_obj_add_event_cb(back, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* title = lv_label_create(hdr);
        lv_label_set_text(title, LV_SYMBOL_EYE_OPEN " System Monitor");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontIcon, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildScrollArea() {
        _scroll = lv_obj_create(_screen);
        lv_obj_set_size(_scroll, W, CONTENT_H);
        lv_obj_set_pos(_scroll, 0, HEADER_H);
        lv_obj_set_style_bg_color(_scroll, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_scroll, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_scroll, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(_scroll, 6, LV_PART_MAIN);
        lv_obj_set_scroll_dir(_scroll, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(_scroll, LV_SCROLLBAR_MODE_AUTO);
        lv_obj_set_flex_flow(_scroll, LV_FLEX_FLOW_COLUMN);
    }

    // Heap history chart - the centerpiece of the screen
    void _buildHeapChart() {
        lv_obj_t* card = _makeCard(_scroll, W - 12, 110);

        lv_obj_t* titleLbl = lv_label_create(card);
        lv_label_set_text(titleLbl, "Heap Memory");
        lv_obj_set_style_text_color(titleLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(titleLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(titleLbl, 6, 4);

        _heapPctLbl = lv_label_create(card);
        lv_label_set_text(_heapPctLbl, "-- %");
        lv_obj_set_style_text_color(_heapPctLbl, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(_heapPctLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_heapPctLbl, LV_ALIGN_TOP_RIGHT, -6, 4);

        _heapLbl = lv_label_create(card);
        lv_label_set_text(_heapLbl, "-- / -- KB");
        lv_obj_set_style_text_color(_heapLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_heapLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(_heapLbl, 6, 18);

        _chart = lv_chart_create(card);
        lv_obj_set_size(_chart, (W - 12) - 12, 68);
        lv_obj_set_pos(_chart, 6, 34);
        lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(_chart, CHART_POINTS);
        lv_chart_set_div_line_count(_chart, 3, 0);
        lv_obj_set_style_bg_color(_chart, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_chart, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_chart, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_chart, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_chart, 4, LV_PART_MAIN);
        lv_obj_set_style_size(_chart, 0, LV_PART_INDICATOR);

        // Grid lines
        lv_obj_set_style_line_color(_chart, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_line_opa(_chart, LV_OPA_30, LV_PART_MAIN);

        uint32_t total = ESP.getHeapSize();
        lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, 0, (lv_coord_t)total);

        _heapSeries = lv_chart_add_series(
            _chart, gTheme->primary, LV_CHART_AXIS_PRIMARY_Y);

        // Paint the history that accumulated before this screen opened
        _paintHeapHistory();
    }

    // Pull the global heap history into the chart series. Slots that have
    // never been sampled yet are hidden so the line does not collapse to 0.
    void _paintHeapHistory() {
        static_assert(CHART_POINTS == SysMonHistory::CAPACITY,
            "Chart point count must match history capacity");
        uint32_t hist[SysMonHistory::CAPACITY];
        SysMonHistory& h = SysMonHistory::getInstance();
        h.snapshot(hist);
        int pad = SysMonHistory::CAPACITY - h.validCount();
        for (int i = 0; i < SysMonHistory::CAPACITY; i++) {
            if (i < pad) {
                lv_chart_set_value_by_id(_chart, _heapSeries, i, LV_CHART_POINT_NONE);
            } else {
                lv_chart_set_value_by_id(_chart, _heapSeries, i, (lv_coord_t)hist[i]);
            }
        }
        lv_chart_refresh(_chart);
    }

    // 2x2 stat grid: CPU, Uptime, WiFi signal, IP
    void _buildStatGrid() {
        lv_obj_t* grid = lv_obj_create(_scroll);
        lv_obj_set_size(grid, W - 12, 84);
        lv_obj_set_style_bg_color(grid, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_column(grid, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_row(grid, 6, LV_PART_MAIN);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

        int cardW = (W - 12 - 6) / 2;

        // CPU card
        lv_obj_t* cpuCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(cpuCard, 0, 0);
        _cpuLbl = _makeStatLabel(cpuCard, LV_SYMBOL_LOOP " CPU", "--");

        // Uptime card
        lv_obj_t* upCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(upCard, cardW + 6, 0);
        _uptimeLbl = _makeStatLabel(upCard, LV_SYMBOL_REFRESH " Uptime", "--");

        // WiFi card
        lv_obj_t* wifiCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(wifiCard, 0, 44);
        _wifiLbl = _makeStatLabel(wifiCard, LV_SYMBOL_WIFI " Signal", "--");

        // IP card
        lv_obj_t* ipCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(ipCard, cardW + 6, 44);
        _ipLbl = _makeStatLabel(ipCard, LV_SYMBOL_HOME " IP", "--");
    }

    // SD card usage bar
    void _buildSdBar() {
        lv_obj_t* card = _makeCard(_scroll, W - 12, 52);

        lv_obj_t* titleLbl = lv_label_create(card);
        lv_label_set_text(titleLbl, LV_SYMBOL_SD_CARD " SD Card");
        lv_obj_set_style_text_color(titleLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(titleLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(titleLbl, 6, 5);

        _sdLbl = lv_label_create(card);
        lv_label_set_text(_sdLbl, "--");
        lv_obj_set_style_text_color(_sdLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_sdLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_sdLbl, LV_ALIGN_TOP_RIGHT, -6, 5);

        // Progress bar for used/total
        _sdBar = lv_bar_create(card);
        lv_obj_set_size(_sdBar, (W - 12) - 12, 10);
        lv_obj_set_pos(_sdBar, 6, 34);
        lv_bar_set_range(_sdBar, 0, 100);
        lv_bar_set_value(_sdBar, 0, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(_sdBar, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_sdBar, gTheme->primary, LV_PART_INDICATOR);
        lv_obj_set_style_radius(_sdBar, 5, LV_PART_MAIN);
        lv_obj_set_style_radius(_sdBar, 5, LV_PART_INDICATOR);
    }

    // Bottom row: temperature + task count
    void _buildBottomRow() {
        lv_obj_t* grid = lv_obj_create(_scroll);
        lv_obj_set_size(grid, W - 12, 38);
        lv_obj_set_style_bg_color(grid, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
        lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

        int cardW = (W - 12 - 6) / 2;

        lv_obj_t* tempCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(tempCard, 0, 0);
        _tempLbl = _makeStatLabel(tempCard, LV_SYMBOL_WARNING " Temp", "--");

        lv_obj_t* tasksCard = _makeCard(grid, cardW, 38);
        lv_obj_set_pos(tasksCard, cardW + 6, 0);
        _tasksLbl = _makeStatLabel(tasksCard, LV_SYMBOL_LIST " Tasks", "--");
    }

    // Creates a styled card container
    lv_obj_t* _makeCard(lv_obj_t* parent, int w, int h) {
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, w, h);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(card, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(card, LV_OPA_20, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        return card;
    }

    // Creates a two-line label pair (caption + value) inside a card.
    // Returns the value label so callers can update it on refresh.
    lv_obj_t* _makeStatLabel(lv_obj_t* card, const char* caption, const char* initial) {
        lv_obj_t* cap = lv_label_create(card);
        lv_label_set_text(cap, caption);
        lv_obj_set_style_text_color(cap, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(cap, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(cap, 6, 4);

        lv_obj_t* val = lv_label_create(card);
        lv_label_set_text(val, initial);
        lv_obj_set_style_text_color(val, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(val, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(val, 6, 20);
        return val;
    }

    // Called every second. Reads all metrics and updates LVGL widgets.
    void _refresh() {
        _refreshHeap();
        _refreshCpu();
        _refreshUptime();
        _refreshWifi();
        _refreshSd();
        _refreshTemp();
        _refreshTasks();
    }

    void _refreshHeap() {
        uint32_t total = ESP.getHeapSize();
        uint32_t free = SysMonHistory::getInstance().latest();
        uint32_t used = total - free;
        int pct = (total > 0) ? (int)((used * 100UL) / total) : 0;

        _paintHeapHistory();

        char buf[32];
        snprintf(buf, sizeof(buf), "%u / %u KB", free / 1024, total / 1024);
        _setIfChanged(_heapLbl, buf);

        snprintf(buf, sizeof(buf), "%d%% used", pct);
        _setIfChanged(_heapPctLbl, buf);

        // Color the percentage by pressure: green < 60%, yellow < 80%, red >= 80%
        lv_color_t c = gTheme->successText;
        if (pct >= 80) c = gTheme->errorText;
        else if (pct >= 60) c = gTheme->alertText;
        lv_obj_set_style_text_color(_heapPctLbl, c, LV_PART_MAIN);

        // Chart series color matches the pressure level too
        lv_obj_set_style_line_color(_chart, c, LV_PART_ITEMS);
    }

    void _refreshCpu() {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u MHz", (unsigned)ESP.getCpuFreqMHz());
        _setIfChanged(_cpuLbl, buf);
    }

    void _refreshUptime() {
        unsigned long ms = millis();
        unsigned long s = ms / 1000;
        unsigned long m = s / 60;
        unsigned long h = m / 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h % 24, m % 60, s % 60);
        _setIfChanged(_uptimeLbl, buf);
    }

    void _refreshWifi() {
        if (!WifiManager::getInstance().connected()) {
            _setIfChanged(_wifiLbl, "Offline");
            lv_obj_set_style_text_color(_wifiLbl, gTheme->errorText, LV_PART_MAIN);
            _setIfChanged(_ipLbl, "--");
            return;
        }
        int rssi = WiFi.RSSI();
        char buf[16];
        snprintf(buf, sizeof(buf), "%d dBm", rssi);
        _setIfChanged(_wifiLbl, buf);

        // Color RSSI: > -60 good, > -75 ok, worse = bad
        lv_color_t c = gTheme->successText;
        if (rssi < -75) c = gTheme->errorText;
        else if (rssi < -60) c = gTheme->alertText;
        lv_obj_set_style_text_color(_wifiLbl, c, LV_PART_MAIN);

        String ip = WifiManager::getInstance().ip();
        _setIfChanged(_ipLbl, ip.c_str());
    }

    void _refreshSd() {
        if (!SDManager::getInstance().isMounted()) {
            _setIfChanged(_sdLbl, "Not mounted");
            lv_bar_set_value(_sdBar, 0, LV_ANIM_OFF);
            return;
        }
        uint64_t total = SD.totalBytes();
        uint64_t used = SD.usedBytes();
        int pct = (total > 0) ? (int)((used * 100ULL) / total) : 0;

        char buf[32];
        snprintf(buf, sizeof(buf), "%llu / %llu MB",
                 (unsigned long long)(used / 1048576ULL),
                 (unsigned long long)(total / 1048576ULL));
        _setIfChanged(_sdLbl, buf);
        lv_bar_set_value(_sdBar, pct, LV_ANIM_OFF);

        // Color bar by usage
        lv_color_t c = gTheme->primary;
        if (pct >= 90) c = gTheme->errorText;
        else if (pct >= 75) c = gTheme->alertText;
        lv_obj_set_style_bg_color(_sdBar, c, LV_PART_INDICATOR);
    }

    void _refreshTemp() {
        // temperatureRead() is declared in Arduino.h (esp32-hal-misc.h) and
        // returns the internal die temperature in Celsius. Not calibrated,
        // but useful as a relative thermal indicator.
        float tempC = temperatureRead();
        char buf[12];
        snprintf(buf, sizeof(buf), "%.1f C", (double)tempC);
        _setIfChanged(_tempLbl, buf);

        lv_color_t c = gTheme->successText;
        if (tempC >= 70.0f) c = gTheme->errorText;
        else if (tempC >= 55.0f) c = gTheme->alertText;
        lv_obj_set_style_text_color(_tempLbl, c, LV_PART_MAIN);
    }

    void _refreshTasks() {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", (unsigned)uxTaskGetNumberOfTasks());
        _setIfChanged(_tasksLbl, buf);
    }

    // Only sets the label text when it actually changed to avoid
    // triggering unnecessary LVGL redraws every second.
    static void _setIfChanged(lv_obj_t* lbl, const char* text) {
        if (!lbl) return;
        const char* cur = lv_label_get_text(lbl);
        if (cur && strcmp(cur, text) == 0) return;
        lv_label_set_text(lbl, text);
    }
};
