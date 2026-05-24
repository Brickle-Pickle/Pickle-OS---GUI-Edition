#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../network/wifi_manager.h"
#include "../../network/html_fetcher.h"

// BrowserScreen - Plain-text web browser.
class BrowserScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildUrlBar();
        _buildBookmarks();
        _buildContent();
        _buildKeyboard();
    }

    void onDestroy() override {
        _cancelPoll();
        if (_shared) {
            _shared->screen_alive = false;
            _shared = nullptr;
        }
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    // Shared block between LVGL thread and HTTP worker. Heap-allocated so it
    // survives screen destruction if a fetch is still in flight.
    struct Shared {
        volatile bool job_done = false;
        volatile bool screen_alive = true;
        String url;
        HtmlFetcher::Result result;
    };

    Shared* _shared = nullptr;
    lv_obj_t* _urlTa = nullptr;
    lv_obj_t* _contentLabel = nullptr;
    lv_obj_t* _scrollContainer = nullptr;
    lv_obj_t* _statusBar = nullptr;
    lv_obj_t* _statusLbl = nullptr;
    lv_obj_t* _kb = nullptr;
    lv_timer_t* _pollTmr = nullptr;

    static constexpr int W = 240;
    static constexpr int H = 320;
    static constexpr int HEADER_H = 32;
    static constexpr int URL_BAR_H = 30;
    static constexpr int BOOKMARKS_H = 28;
    static constexpr int STATUS_H = 18;
    static constexpr int KB_H = 140;
    static constexpr int CONTENT_Y = HEADER_H + URL_BAR_H + BOOKMARKS_H;
    static constexpr int CONTENT_H = H - CONTENT_Y - STATUS_H;


    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, W, HEADER_H);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* backBtn = lv_btn_create(header);
        lv_obj_set_size(backBtn, 32, 26);
        lv_obj_align(backBtn, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(backBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(backBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(backBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(backBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(backBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* backIco = lv_label_create(backBtn);
        lv_label_set_text(backIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(backIco, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(backIco, gFontIcon, LV_PART_MAIN);
        lv_obj_center(backIco);
        lv_obj_add_event_cb(backBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, LV_SYMBOL_DOWNLOAD " Browser");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontIcon, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t* goBtn = lv_btn_create(header);
        lv_obj_set_size(goBtn, 36, 26);
        lv_obj_align(goBtn, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(goBtn, gTheme->secondaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(goBtn, gTheme->secondary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(goBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(goBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(goBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(goBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* goLbl = lv_label_create(goBtn);
        lv_label_set_text(goLbl, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(goLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(goLbl, gFontIcon, LV_PART_MAIN);
        lv_obj_center(goLbl);
        lv_obj_add_event_cb(goBtn, [](lv_event_t* e) {
            ((BrowserScreen*)lv_event_get_user_data(e))->_navigate();
        }, LV_EVENT_CLICKED, this);
    }

    void _buildUrlBar() {
        lv_obj_t* bar = lv_obj_create(_screen);
        lv_obj_set_size(bar, W, URL_BAR_H);
        lv_obj_set_pos(bar, 0, HEADER_H);
        lv_obj_set_style_bg_color(bar, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 2, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        _urlTa = lv_textarea_create(bar);
        lv_textarea_set_one_line(_urlTa, true);
        lv_textarea_set_placeholder_text(_urlTa, "http://...");
        lv_obj_set_size(_urlTa, W - 4, URL_BAR_H - 4);
        lv_obj_set_pos(_urlTa, 0, 0);
        lv_obj_set_style_bg_color(_urlTa, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(_urlTa, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_urlTa, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_urlTa, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_urlTa, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_urlTa, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_urlTa, 3, LV_PART_MAIN);
        lv_obj_add_event_cb(_urlTa, [](lv_event_t* e) {
            BrowserScreen* s = (BrowserScreen*)lv_event_get_user_data(e);
            s->_showKeyboard(s->_urlTa);
        }, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(_urlTa, [](lv_event_t* e) {
            BrowserScreen* s = (BrowserScreen*)lv_event_get_user_data(e);
            s->_showKeyboard(s->_urlTa);
        }, LV_EVENT_FOCUSED, this);
    }

    void _buildBookmarks() {
        struct Bookmark { const char* label; const char* url; };
        static const Bookmark bookmarks[] = {
            { "Brickle", "https://bricklepickle.com/" },
            { "wttr.in", "http://wttr.in/?format=3" },
            { "HN", "http://news.ycombinator.com" },
            { "example", "http://example.com" },
            { "ifconfig","http://ifconfig.me/all" },
        };
        static constexpr int count = 5;

        lv_obj_t* bar = lv_obj_create(_screen);
        lv_obj_set_size(bar, W, BOOKMARKS_H);
        lv_obj_set_pos(bar, 0, HEADER_H + URL_BAR_H);
        lv_obj_set_style_bg_color(bar, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 3, LV_PART_MAIN);
        lv_obj_set_style_pad_column(bar, 4, LV_PART_MAIN);
        lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < count; i++) {
            lv_obj_t* btn = lv_btn_create(bar);
            lv_obj_set_height(btn, BOOKMARKS_H - 6);
            lv_obj_set_style_bg_color(btn, gTheme->primaryDark, LV_PART_MAIN);
            lv_obj_set_style_bg_color(btn, gTheme->primary, LV_STATE_PRESSED);
            lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
            lv_obj_set_style_pad_hor(btn, 6, LV_PART_MAIN);
            lv_obj_set_style_pad_ver(btn, 2, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, bookmarks[i].label);
            lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);

            // Store bookmark URL pointer in the button's user data.
            // bookmarks[] is a local static so its lifetime is the program.
            lv_obj_set_user_data(btn, (void*)bookmarks[i].url);
            lv_obj_add_event_cb(btn, [](lv_event_t* e) {
                BrowserScreen* s = (BrowserScreen*)lv_event_get_user_data(e);
                const char* url = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
                lv_textarea_set_text(s->_urlTa, url);
                s->_navigate();
            }, LV_EVENT_CLICKED, this);
        }
    }

    void _buildContent() {
        // Scrollable container for the page text
        _scrollContainer = lv_obj_create(_screen);
        lv_obj_set_size(_scrollContainer, W, CONTENT_H);
        lv_obj_set_pos(_scrollContainer, 0, CONTENT_Y);
        lv_obj_set_style_bg_color(_scrollContainer, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_scrollContainer, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_scrollContainer, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_scrollContainer, 4, LV_PART_MAIN);
        lv_obj_set_scroll_dir(_scrollContainer, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(_scrollContainer, LV_SCROLLBAR_MODE_AUTO);

        _contentLabel = lv_label_create(_scrollContainer);
        lv_label_set_text(_contentLabel, "Enter a URL above and press " LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(_contentLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_contentLabel, gFontSmall, LV_PART_MAIN);
        lv_obj_set_width(_contentLabel, W - 12);
        lv_label_set_long_mode(_contentLabel, LV_LABEL_LONG_WRAP);

        // Status bar at the bottom
        _statusBar = lv_obj_create(_screen);
        lv_obj_set_size(_statusBar, W, STATUS_H);
        lv_obj_set_pos(_statusBar, 0, H - STATUS_H);
        lv_obj_set_style_bg_color(_statusBar, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(_statusBar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_statusBar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_statusBar, 2, LV_PART_MAIN);
        lv_obj_clear_flag(_statusBar, LV_OBJ_FLAG_SCROLLABLE);

        _statusLbl = lv_label_create(_statusBar);
        lv_label_set_text(_statusLbl, "Ready");
        lv_obj_set_style_text_color(_statusLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_statusLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_set_width(_statusLbl, W - 8);
        lv_label_set_long_mode(_statusLbl, LV_LABEL_LONG_DOT);
        lv_obj_align(_statusLbl, LV_ALIGN_LEFT_MID, 2, 0);
    }

    void _buildKeyboard() {
        _kb = lv_keyboard_create(_screen);
        lv_obj_set_size(_kb, W, KB_H);
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_mode(_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(_kb, [](lv_event_t* e) {
            BrowserScreen* s = (BrowserScreen*)lv_event_get_user_data(e);
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_READY) {
                s->_hideKeyboard();
                s->_navigate();
            } else if (code == LV_EVENT_CANCEL) {
                s->_hideKeyboard();
            }
        }, LV_EVENT_ALL, this);
    }

    void _showKeyboard(lv_obj_t* ta) {
        if (!_kb) return;
        lv_keyboard_set_textarea(_kb, ta);
        lv_obj_clear_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_kb);
        // Shrink content area while keyboard is visible
        lv_obj_set_height(_scrollContainer, CONTENT_H - KB_H);
    }

    void _hideKeyboard() {
        if (!_kb) return;
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_height(_scrollContainer, CONTENT_H);
    }

    void _setStatus(const char* text, lv_color_t color) {
        if (!_statusLbl) return;
        lv_label_set_text(_statusLbl, text);
        lv_obj_set_style_text_color(_statusLbl, color, LV_PART_MAIN);
    }

    void _navigate() {
        if (!WifiManager::getInstance().connected()) {
            ToastManager::getInstance().showToast("WiFi not connected", ToastType::ALERT);
            return;
        }
        const char* url = lv_textarea_get_text(_urlTa);
        if (!url || !*url) {
            ToastManager::getInstance().showToast("Enter a URL first", ToastType::ALERT);
            return;
        }

        // Reject obviously broken URLs
        String urlStr = String(url);
        urlStr.trim();
        if (!urlStr.startsWith("http://") && !urlStr.startsWith("https://")) {
            urlStr = "http://" + urlStr;
            lv_textarea_set_text(_urlTa, urlStr.c_str());
        }

        if (_shared && !_shared->job_done) {
            ToastManager::getInstance().showToast("Fetch in progress...", ToastType::INFO);
            return;
        }
        if (_shared) { delete _shared; _shared = nullptr; }

        _hideKeyboard();

        lv_label_set_text(_contentLabel, "Loading...");
        lv_obj_set_style_text_color(_contentLabel, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_scroll_to_y(_scrollContainer, 0, LV_ANIM_OFF);
        _setStatus("Fetching...", gTheme->alertText);

        _shared = new Shared();
        _shared->url = urlStr;

        // Worker runs on core 0 (WiFi core). 20 KB stack: TLS needs ~14 KB alone.
        xTaskCreatePinnedToCore(_workerTask, "browser-fetch", 20480, _shared, 1, nullptr, 0);
        _cancelPoll();
        _pollTmr = lv_timer_create([](lv_timer_t* t) {
            ((BrowserScreen*)t->user_data)->_pollResult();
        }, 150, this);
    }

    static void _workerTask(void* arg) {
        Shared* sh = (Shared*)arg;
        sh->result = HtmlFetcher::fetch(sh->url, 3000);
        sh->job_done = true;
        if (!sh->screen_alive) delete sh;
        vTaskDelete(nullptr);
    }

    void _cancelPoll() {
        if (_pollTmr) { lv_timer_del(_pollTmr); _pollTmr = nullptr; }
    }

    void _pollResult() {
        if (!_shared || !_shared->job_done) return;
        _cancelPoll();
        _renderResult(_shared->result);
        delete _shared;
        _shared = nullptr;
    }

    void _renderResult(const HtmlFetcher::Result& r) {
        if (!r.success) {
            String msg = "Error: " + r.error;
            lv_label_set_text(_contentLabel, msg.c_str());
            lv_obj_set_style_text_color(_contentLabel, gTheme->errorText, LV_PART_MAIN);
            char statusBuf[48];
            snprintf(statusBuf, sizeof(statusBuf), "HTTP %d  %s", r.httpCode, r.error.c_str());
            _setStatus(statusBuf, gTheme->errorText);
            return;
        }

        if (r.text.length() == 0) {
            lv_label_set_text(_contentLabel, "(No readable text found on this page)");
            lv_obj_set_style_text_color(_contentLabel, gTheme->textSoft, LV_PART_MAIN);
            _setStatus("Empty page (JS-rendered?)", gTheme->alertText);
            return;
        }

        lv_label_set_text(_contentLabel, r.text.c_str());
        lv_obj_set_style_text_color(_contentLabel, gTheme->textDark, LV_PART_MAIN);
        lv_obj_scroll_to_y(_scrollContainer, 0, LV_ANIM_OFF);

        char statusBuf[64];
        snprintf(statusBuf, sizeof(statusBuf), "HTTP %d  %u chars  %s",
                 r.httpCode, (unsigned)r.text.length(),
                 r.finalUrl.length() > 32
                     ? ("..." + r.finalUrl.substring(r.finalUrl.length() - 29)).c_str()
                     : r.finalUrl.c_str());
        _setStatus(statusBuf, gTheme->successText);
    }
};

