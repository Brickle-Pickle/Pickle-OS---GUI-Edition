#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../network/wifi_manager.h"

// HttpTesterScreen - Mini Postman style HTTP client.
// Lets the user fire GET, POST or PUT requests against an arbitrary URL,
// attach custom headers and a body, and view the formatted response.
// Networking runs on a dedicated FreeRTOS task to keep LVGL responsive.
class HttpTesterScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildRequestBar();
        _buildTabs();
        _buildKeyboard();
    }

    void onDestroy() override {
        _cancelPollTimer();
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
    // Shared block between LVGL thread and HTTP worker. Lives on the heap so it
    // can outlive the screen if the user navigates away while a request is in
    // flight. The worker frees it after detecting screen_alive == false.
    struct Shared {
        volatile bool job_done = false;
        volatile bool screen_alive = true;
        String method;
        String url;
        String headers;
        String body;
        int    status = 0;
        String responseBody;
        String error;
    };

    Shared*     _shared = nullptr;
    lv_obj_t*   _methodDd = nullptr;
    lv_obj_t*   _urlTa = nullptr;
    lv_obj_t*   _tabview = nullptr;
    lv_obj_t*   _headersTa = nullptr;
    lv_obj_t*   _bodyTa = nullptr;
    lv_obj_t*   _responseTa = nullptr;
    lv_obj_t*   _statusLbl = nullptr;
    lv_obj_t*   _sendBtn = nullptr;
    lv_obj_t*   _kb = nullptr;
    lv_timer_t* _pollTmr = nullptr;
    bool        _kbVisible = false;

    static constexpr int W = 240;
    static constexpr int H = 320;
    static constexpr int HEADER_H = 32;
    static constexpr int REQ_BAR_H = 32;
    static constexpr int TAB_Y = HEADER_H + REQ_BAR_H;
    static constexpr int KB_H = 140;

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
        lv_obj_t* bIco = lv_label_create(backBtn);
        lv_label_set_text(bIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(bIco, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(bIco, gFontIcon, LV_PART_MAIN);
        lv_obj_center(bIco);
        lv_obj_add_event_cb(backBtn, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "HTTP Tester");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        _sendBtn = lv_btn_create(header);
        lv_obj_set_size(_sendBtn, 48, 26);
        lv_obj_align(_sendBtn, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(_sendBtn, gTheme->secondaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(_sendBtn, gTheme->secondary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(_sendBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_sendBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(_sendBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_sendBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* sLbl = lv_label_create(_sendBtn);
        lv_label_set_text(sLbl, "Send");
        lv_obj_set_style_text_color(sLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(sLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(sLbl);
        lv_obj_add_event_cb(_sendBtn, [](lv_event_t* e) {
            HttpTesterScreen* s = (HttpTesterScreen*)lv_event_get_user_data(e);
            s->_send();
        }, LV_EVENT_CLICKED, this);
    }

    void _buildRequestBar() {
        // Method dropdown on the left, URL textarea filling the rest
        _methodDd = lv_dropdown_create(_screen);
        lv_dropdown_set_options(_methodDd, "GET\nPOST\nPUT");
        lv_obj_set_size(_methodDd, 64, REQ_BAR_H - 4);
        lv_obj_set_pos(_methodDd, 4, HEADER_H + 2);
        lv_obj_set_style_bg_color(_methodDd, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(_methodDd, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_methodDd, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_methodDd, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_methodDd, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_methodDd, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_methodDd, 4, LV_PART_MAIN);

        _urlTa = lv_textarea_create(_screen);
        lv_textarea_set_one_line(_urlTa, true);
        lv_textarea_set_placeholder_text(_urlTa, "https://...");
        lv_obj_set_size(_urlTa, W - 64 - 12, REQ_BAR_H - 4);
        lv_obj_set_pos(_urlTa, 72, HEADER_H + 2);
        lv_obj_set_style_bg_color(_urlTa, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(_urlTa, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_urlTa, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_urlTa, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_urlTa, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_urlTa, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_urlTa, 4, LV_PART_MAIN);
        lv_obj_add_event_cb(_urlTa, _onTextareaFocus, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(_urlTa, _onTextareaFocus, LV_EVENT_CLICKED, this);
    }

    void _buildTabs() {
        _tabview = lv_tabview_create(_screen, LV_DIR_TOP, 26);
        lv_obj_set_size(_tabview, W, H - TAB_Y);
        lv_obj_set_pos(_tabview, 0, TAB_Y);
        lv_obj_set_style_bg_color(_tabview, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_text_font(_tabview, gFontSmall, LV_PART_MAIN);

        lv_obj_t* btns = lv_tabview_get_tab_btns(_tabview);
        lv_obj_set_style_bg_color(btns, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(btns, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_color(btns, gTheme->textDark, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btns, gTheme->primaryDark, LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_font(btns, gFontSmall, LV_PART_MAIN);

        lv_obj_t* tHeaders = lv_tabview_add_tab(_tabview, "Headers");
        lv_obj_t* tBody = lv_tabview_add_tab(_tabview, "Body");
        lv_obj_t* tResp = lv_tabview_add_tab(_tabview, "Response");
        lv_obj_set_style_pad_all(tHeaders, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tBody, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_all(tResp, 2, LV_PART_MAIN);

        _headersTa = _makeMultilineTa(tHeaders, "Key: Value\nOne per line");
        _bodyTa = _makeMultilineTa(tBody, "Request body");

        // Response tab: status label on top + read-only textarea below
        _statusLbl = lv_label_create(tResp);
        lv_label_set_text(_statusLbl, "Not sent");
        lv_obj_set_style_text_color(_statusLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_statusLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(_statusLbl, 4, 0);

        _responseTa = lv_textarea_create(tResp);
        lv_textarea_set_one_line(_responseTa, false);
        lv_textarea_set_placeholder_text(_responseTa, "Send a request to see the response here");
        lv_obj_set_size(_responseTa, W - 8, H - TAB_Y - 26 - 22);
        lv_obj_set_pos(_responseTa, 0, 16);
        lv_obj_set_style_bg_color(_responseTa, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(_responseTa, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(_responseTa, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(_responseTa, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(_responseTa, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(_responseTa, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_responseTa, 4, LV_PART_MAIN);
        // Read-only: do not raise the keyboard when tapped
    }

    lv_obj_t* _makeMultilineTa(lv_obj_t* parent, const char* placeholder) {
        lv_obj_t* ta = lv_textarea_create(parent);
        lv_textarea_set_one_line(ta, false);
        lv_textarea_set_placeholder_text(ta, placeholder);
        lv_obj_set_size(ta, W - 8, H - TAB_Y - 26 - 6);
        lv_obj_set_pos(ta, 0, 0);
        lv_obj_set_style_bg_color(ta, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_text_color(ta, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(ta, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(ta, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(ta, 4, LV_PART_MAIN);
        lv_obj_add_event_cb(ta, _onTextareaFocus, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(ta, _onTextareaFocus, LV_EVENT_CLICKED, this);
        return ta;
    }

    void _buildKeyboard() {
        _kb = lv_keyboard_create(_screen);
        lv_obj_set_size(_kb, W, KB_H);
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_mode(_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        // Hook the OK and CLOSE events to dismiss the keyboard cleanly
        lv_obj_add_event_cb(_kb, [](lv_event_t* e) {
            HttpTesterScreen* s = (HttpTesterScreen*)lv_event_get_user_data(e);
            lv_event_code_t code = lv_event_get_code(e);
            if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
                s->_hideKeyboard();
            }
        }, LV_EVENT_ALL, this);
    }

    static void _onTextareaFocus(lv_event_t* e) {
        HttpTesterScreen* s = (HttpTesterScreen*)lv_event_get_user_data(e);
        lv_obj_t* ta = lv_event_get_target(e);
        s->_showKeyboard(ta);
    }

    void _showKeyboard(lv_obj_t* ta) {
        if (!_kb) return;
        lv_keyboard_set_textarea(_kb, ta);
        lv_obj_clear_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(_kb);
        _kbVisible = true;
        // Shrink the tabview while the keyboard is visible
        lv_obj_set_height(_tabview, H - TAB_Y - KB_H);
        _resizeTabContent(H - TAB_Y - KB_H - 26);
    }

    void _hideKeyboard() {
        if (!_kb) return;
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        _kbVisible = false;
        lv_obj_set_height(_tabview, H - TAB_Y);
        _resizeTabContent(H - TAB_Y - 26);
    }

    void _resizeTabContent(int innerH) {
        if (_headersTa) lv_obj_set_height(_headersTa, innerH - 6);
        if (_bodyTa) lv_obj_set_height(_bodyTa, innerH - 6);
        if (_responseTa) lv_obj_set_height(_responseTa, innerH - 22);
    }

    void _cancelPollTimer() {
        if (_pollTmr) { lv_timer_del(_pollTmr); _pollTmr = nullptr; }
    }

    void _send() {
        if (!WifiManager::getInstance().connected()) {
            ToastManager::getInstance().showToast("WiFi not connected", ToastType::ALERT);
            return;
        }
        const char* url = lv_textarea_get_text(_urlTa);
        if (!url || !*url) {
            ToastManager::getInstance().showToast("URL is empty", ToastType::ALERT);
            return;
        }
        if (_shared && !_shared->job_done) {
            ToastManager::getInstance().showToast("Request in progress", ToastType::INFO);
            return;
        }
        if (_shared) { delete _shared; _shared = nullptr; }

        _hideKeyboard();
        lv_tabview_set_act(_tabview, 2, LV_ANIM_OFF);
        if (_statusLbl) {
            lv_label_set_text(_statusLbl, "Sending...");
            lv_obj_set_style_text_color(_statusLbl, gTheme->alertText, LV_PART_MAIN);
        }
        lv_textarea_set_text(_responseTa, "");

        _shared = new Shared();
        _shared->method = _methodString();
        _shared->url = url;
        _shared->headers = lv_textarea_get_text(_headersTa);
        _shared->body = lv_textarea_get_text(_bodyTa);

        // 16 KB stack: HTTPClient plus TLS handshake fit with margin
        xTaskCreatePinnedToCore(_workerTask, "http-test", 16384, _shared, 1, nullptr, 0);
        _cancelPollTimer();
        _pollTmr = lv_timer_create([](lv_timer_t* t) {
            ((HttpTesterScreen*)t->user_data)->_pollResult();
        }, 100, this);
    }

    String _methodString() {
        char buf[8];
        lv_dropdown_get_selected_str(_methodDd, buf, sizeof(buf));
        return String(buf);
    }

    static void _workerTask(void* arg) {
        Shared* sh = (Shared*)arg;
        HTTPClient http;
        http.setTimeout(20000);
        http.setConnectTimeout(10000);
        http.setReuse(false);

        bool began;
        if (sh->url.startsWith("https://")) {
            // Skip cert validation: this is a developer tool, not production
            static WiFiClientSecure secure;
            secure.setInsecure();
            began = http.begin(secure, sh->url);
        } else {
            began = http.begin(sh->url);
        }
        if (!began) {
            sh->error = "Failed to parse URL";
            sh->job_done = true;
            if (!sh->screen_alive) delete sh;
            vTaskDelete(nullptr);
            return;
        }

        // Parse "Key: Value" lines into HTTPClient headers
        int start = 0;
        while (start < (int)sh->headers.length()) {
            int nl = sh->headers.indexOf('\n', start);
            if (nl < 0) nl = sh->headers.length();
            String line = sh->headers.substring(start, nl);
            line.trim();
            if (line.length()) {
                int colon = line.indexOf(':');
                if (colon > 0) {
                    String k = line.substring(0, colon);
                    String v = line.substring(colon + 1);
                    k.trim(); v.trim();
                    if (k.length()) http.addHeader(k, v);
                }
            }
            start = nl + 1;
        }

        int code;
        if (sh->method == "POST") {
            code = http.POST((uint8_t*)sh->body.c_str(), sh->body.length());
        } else if (sh->method == "PUT") {
            code = http.PUT((uint8_t*)sh->body.c_str(), sh->body.length());
        } else {
            code = http.GET();
        }
        sh->status = code;
        if (code > 0) {
            sh->responseBody = http.getString();
        } else {
            sh->error = HTTPClient::errorToString(code);
        }
        http.end();

        sh->job_done = true;
        if (!sh->screen_alive) delete sh;
        vTaskDelete(nullptr);
    }

    void _pollResult() {
        if (!_shared || !_shared->job_done) return;
        _cancelPollTimer();
        _renderResult(*_shared);
        delete _shared;
        _shared = nullptr;
    }

    void _renderResult(const Shared& sh) {
        if (sh.status <= 0) {
            String msg = "Error";
            if (sh.error.length()) msg += ": " + sh.error;
            lv_label_set_text(_statusLbl, msg.c_str());
            lv_obj_set_style_text_color(_statusLbl, gTheme->errorText, LV_PART_MAIN);
            lv_textarea_set_text(_responseTa, "");
            return;
        }
        char status[64];
        snprintf(status, sizeof(status), "HTTP %d  (%u bytes)",
                 sh.status, (unsigned)sh.responseBody.length());
        lv_label_set_text(_statusLbl, status);
        lv_color_t c = gTheme->textSoft;
        if (sh.status >= 200 && sh.status < 300) c = gTheme->successText;
        else if (sh.status >= 400) c = gTheme->errorText;
        else c = gTheme->alertText;
        lv_obj_set_style_text_color(_statusLbl, c, LV_PART_MAIN);

        String pretty = _prettyPrint(sh.responseBody);
        // Cap response display at 4 KB to avoid blowing LVGL textarea memory
        if (pretty.length() > 4096) {
            pretty.remove(4096);
            pretty += "\n... [truncated]";
        }
        lv_textarea_set_text(_responseTa, pretty.c_str());
    }

    // Try to parse the body as JSON and pretty-print it. Falls back to the
    // original string when parsing fails or the payload is too large.
    static String _prettyPrint(const String& raw) {
        if (raw.length() == 0) return raw;
        if (raw.length() > 8192) return raw;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, raw);
        if (err) return raw;
        String out;
        serializeJsonPretty(doc, out);
        return out;
    }
};
