#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../storage/sd_manager.h"
#include "../../crypto/totp.h"

// TotpScreen - RFC 6238 authenticator. Stores accounts on the SD card as
// "name|base32secret" lines, recomputes each 6-digit code only when the
// 30-second window rolls over, and shows a countdown bar per account.
class TotpScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _loadAccounts();
        _buildHeader();
        _buildList();
        _refreshAll();

        _tickTimer = lv_timer_create(_tickCb, 1000, this);
    }

    void onDestroy() override {
        if (_tickTimer) { lv_timer_del(_tickTimer); _tickTimer = nullptr; }
        if (_screen) { lv_obj_del(_screen); _screen = nullptr; }
    }

private:
    struct Account {
        String name;
        String secretB32;
        std::vector<uint8_t> key;
        lv_obj_t* card = nullptr;
        lv_obj_t* codeLbl = nullptr;
        lv_obj_t* bar = nullptr;
        int32_t  lastCode = -1;
        uint64_t lastCounter = 0;
    };

    static constexpr int W = 240;
    static constexpr int H = 320;
    static constexpr int HEADER_H = 36;
    static constexpr int CARD_H = 60;
    static constexpr int CARD_GAP = 6;
    static constexpr const char* STORE_PATH = "/pickle-os/sys/totp.txt";

    std::vector<Account> _accounts;
    lv_obj_t*   _list = nullptr;
    lv_timer_t* _tickTimer = nullptr;

    lv_obj_t* _dlg = nullptr;
    lv_obj_t* _nameTa = nullptr;
    lv_obj_t* _secretTa = nullptr;
    lv_obj_t* _kb = nullptr;

    lv_obj_t* _delDlg = nullptr;
    int       _delIndex = -1;

    bool _suppressClick = false;

    void _buildHeader() {
        lv_obj_t* header = lv_obj_create(_screen);
        lv_obj_set_size(header, W, HEADER_H);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(header, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* back = lv_btn_create(header);
        lv_obj_set_size(back, 32, 28);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(back, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(back, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(back, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(back, 0, LV_PART_MAIN);
        lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* bIco = lv_label_create(back);
        lv_label_set_text(bIco, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(bIco, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(bIco, gFontIcon, LV_PART_MAIN);
        lv_obj_center(bIco);
        lv_obj_add_event_cb(back, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Authenticator");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t* add = lv_btn_create(header);
        lv_obj_set_size(add, 32, 28);
        lv_obj_align(add, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(add, gTheme->secondaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(add, gTheme->secondary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(add, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(add, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(add, 0, LV_PART_MAIN);
        lv_obj_clear_flag(add, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* plus = lv_label_create(add);
        lv_label_set_text(plus, LV_SYMBOL_PLUS);
        lv_obj_set_style_text_color(plus, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(plus, gFontIcon, LV_PART_MAIN);
        lv_obj_center(plus);
        lv_obj_add_event_cb(add, [](lv_event_t* e) {
            ((TotpScreen*)lv_event_get_user_data(e))->_openAddDialog();
        }, LV_EVENT_CLICKED, this);
    }

    void _buildList() {
        if (_list) { lv_obj_del(_list); _list = nullptr; }
        for (auto& a : _accounts) { a.card = nullptr; a.codeLbl = nullptr; a.bar = nullptr; }

        _list = lv_obj_create(_screen);
        lv_obj_set_size(_list, W, H - HEADER_H);
        lv_obj_set_pos(_list, 0, HEADER_H);
        lv_obj_set_style_bg_color(_list, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_list, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_row(_list, CARD_GAP, LV_PART_MAIN);
        lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(_list, gTheme->primaryDark, LV_PART_SCROLLBAR);
        lv_obj_set_style_bg_opa(_list, LV_OPA_60, LV_PART_SCROLLBAR);
        lv_obj_set_style_width(_list, 4, LV_PART_SCROLLBAR);
        lv_obj_set_style_radius(_list, 2, LV_PART_SCROLLBAR);

        if (_accounts.empty()) {
            lv_obj_t* empty = lv_label_create(_list);
            lv_label_set_text(empty, "No accounts yet.\nTap + to add one.");
            lv_obj_set_style_text_color(empty, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(empty, gFontSmall, LV_PART_MAIN);
            lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_width(empty, W - 32);
            return;
        }

        for (size_t i = 0; i < _accounts.size(); ++i) {
            _buildCard(i);
        }
    }

    void _buildCard(size_t i) {
        Account& a = _accounts[i];

        a.card = lv_obj_create(_list);
        lv_obj_set_size(a.card, W - 24, CARD_H);
        lv_obj_set_style_bg_color(a.card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(a.card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(a.card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(a.card, 8, LV_PART_MAIN);
        lv_obj_clear_flag(a.card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_set_user_data(a.card, (void*)(intptr_t)i);
        lv_obj_add_event_cb(a.card, [](lv_event_t* e) {
            TotpScreen* s = (TotpScreen*)lv_event_get_user_data(e);
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            s->_suppressClick = true;
            s->_openDeleteDialog(idx);
        }, LV_EVENT_LONG_PRESSED, this);
        lv_obj_add_event_cb(a.card, [](lv_event_t* e) {
            TotpScreen* s = (TotpScreen*)lv_event_get_user_data(e);
            if (s->_suppressClick) { s->_suppressClick = false; return; }
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx >= 0 && idx < (int)s->_accounts.size()) {
                ToastManager::getInstance().showToast(s->_accounts[idx].name.c_str(),
                                                     ToastType::INFO, 1500);
            }
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* nameLbl = lv_label_create(a.card);
        lv_label_set_text(nameLbl, a.name.c_str());
        lv_label_set_long_mode(nameLbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(nameLbl, 130);
        lv_obj_set_style_text_color(nameLbl, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(nameLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(nameLbl, LV_ALIGN_TOP_LEFT, 0, 0);

        a.codeLbl = lv_label_create(a.card);
        lv_label_set_text(a.codeLbl, "------");
        lv_obj_set_style_text_color(a.codeLbl, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(a.codeLbl, gFontLarge, LV_PART_MAIN);
        lv_obj_align(a.codeLbl, LV_ALIGN_BOTTOM_LEFT, 0, 2);

        a.bar = lv_bar_create(a.card);
        lv_obj_set_size(a.bar, 50, 6);
        lv_obj_align(a.bar, LV_ALIGN_BOTTOM_RIGHT, 0, -2);
        lv_bar_set_range(a.bar, 0, 30);
        lv_bar_set_value(a.bar, 30, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(a.bar, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_bg_color(a.bar, gTheme->primary, LV_PART_INDICATOR);
        lv_obj_set_style_radius(a.bar, 3, LV_PART_MAIN);
        lv_obj_set_style_radius(a.bar, 3, LV_PART_INDICATOR);

        a.lastCode = -1;
        a.lastCounter = 0;
    }

    static void _tickCb(lv_timer_t* t) {
        ((TotpScreen*)t->user_data)->_refreshAll();
    }

    void _refreshAll() {
        time_t now = time(nullptr);
        bool synced = now >= 1700000000;
        int remaining = synced ? (30 - (int)(now % 30)) : 0;

        for (auto& a : _accounts) {
            if (!a.codeLbl || !a.bar) continue;
            if (!synced) {
                lv_label_set_text(a.codeLbl, "------");
                lv_bar_set_value(a.bar, 0, LV_ANIM_OFF);
                continue;
            }
            uint64_t counter = (uint64_t)now / 30ULL;
            if (counter != a.lastCounter || a.lastCode < 0) {
                int32_t code = totp::totpAt(a.key.data(), a.key.size(), now);
                a.lastCounter = counter;
                a.lastCode = code;
                if (code < 0) {
                    lv_label_set_text(a.codeLbl, "ERR");
                } else {
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%03ld %03ld",
                             (long)(code / 1000), (long)(code % 1000));
                    lv_label_set_text(a.codeLbl, buf);
                }
            }
            lv_bar_set_value(a.bar, remaining, LV_ANIM_OFF);
            lv_color_t c = (remaining <= 5) ? gTheme->errorText : gTheme->primary;
            lv_obj_set_style_bg_color(a.bar, c, LV_PART_INDICATOR);
        }
    }

    void _loadAccounts() {
        _accounts.clear();
        if (!SDManager::getInstance().exists(STORE_PATH)) return;
        String raw = SDManager::getInstance().readFile(STORE_PATH);
        int start = 0;
        while (start < (int)raw.length()) {
            int nl = raw.indexOf('\n', start);
            if (nl < 0) nl = raw.length();
            String line = raw.substring(start, nl);
            start = nl + 1;
            line.trim();
            if (!line.length()) continue;
            int sep = line.indexOf('|');
            if (sep <= 0) continue;
            String name = line.substring(0, sep);
            String sec  = line.substring(sep + 1);
            name.trim(); sec.trim();
            if (!name.length() || !sec.length()) continue;
            _addAccountInternal(name, sec);
        }
    }

    void _saveAccounts() {
        String body;
        for (auto& a : _accounts) {
            body += a.name;
            body += '|';
            body += a.secretB32;
            body += '\n';
        }
        SDManager::getInstance().writeFile(STORE_PATH, body.c_str(), false);
    }

    bool _addAccountInternal(const String& name, const String& secretB32) {
        Account a;
        a.name = name;
        a.secretB32 = secretB32;
        a.key.resize(64);
        size_t n = totp::base32Decode(secretB32.c_str(), a.key.data(), a.key.size());
        if (n == 0) return false;
        a.key.resize(n);
        _accounts.push_back(std::move(a));
        return true;
    }

    void _openAddDialog() {
        if (_dlg) return;
        _dlg = lv_obj_create(_screen);
        lv_obj_set_size(_dlg, W, H);
        lv_obj_set_pos(_dlg, 0, 0);
        lv_obj_set_style_bg_color(_dlg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_dlg, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_border_width(_dlg, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_dlg, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_dlg, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_dlg, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* card = lv_obj_create(_dlg);
        lv_obj_set_size(card, 224, 172);
        lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 6);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* t1 = lv_label_create(card);
        lv_label_set_text(t1, "Account name");
        lv_obj_set_style_text_color(t1, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(t1, gFontSmall, LV_PART_MAIN);
        lv_obj_align(t1, LV_ALIGN_TOP_LEFT, 0, 0);

        _nameTa = lv_textarea_create(card);
        lv_obj_set_size(_nameTa, 204, 28);
        lv_obj_align(_nameTa, LV_ALIGN_TOP_LEFT, 0, 14);
        lv_textarea_set_one_line(_nameTa, true);
        lv_textarea_set_placeholder_text(_nameTa, "GitHub");
        _styleTa(_nameTa);

        lv_obj_t* t2 = lv_label_create(card);
        lv_label_set_text(t2, "Base32 secret");
        lv_obj_set_style_text_color(t2, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(t2, gFontSmall, LV_PART_MAIN);
        lv_obj_align(t2, LV_ALIGN_TOP_LEFT, 0, 50);

        _secretTa = lv_textarea_create(card);
        lv_obj_set_size(_secretTa, 204, 28);
        lv_obj_align(_secretTa, LV_ALIGN_TOP_LEFT, 0, 64);
        lv_textarea_set_one_line(_secretTa, true);
        lv_textarea_set_placeholder_text(_secretTa, "JBSWY3DPEHPK3PXP");
        _styleTa(_secretTa);

        lv_obj_t* cancel = lv_btn_create(card);
        lv_obj_set_size(cancel, 96, 32);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cancel, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(cancel, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cancel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cancel, 0, LV_PART_MAIN);
        lv_obj_clear_flag(cancel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* cLbl = lv_label_create(cancel);
        lv_label_set_text(cLbl, "Cancel");
        lv_obj_set_style_text_color(cLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(cLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(cLbl);
        lv_obj_add_event_cb(cancel, [](lv_event_t* e) {
            ((TotpScreen*)lv_event_get_user_data(e))->_closeAddDialog();
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* save = lv_btn_create(card);
        lv_obj_set_size(save, 96, 32);
        lv_obj_align(save, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(save, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_bg_color(save, gTheme->primary, LV_STATE_PRESSED);
        lv_obj_set_style_radius(save, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(save, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(save, 0, LV_PART_MAIN);
        lv_obj_clear_flag(save, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* sLbl = lv_label_create(save);
        lv_label_set_text(sLbl, "Save");
        lv_obj_set_style_text_color(sLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(sLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(sLbl);
        lv_obj_add_event_cb(save, [](lv_event_t* e) {
            ((TotpScreen*)lv_event_get_user_data(e))->_confirmAdd();
        }, LV_EVENT_CLICKED, this);

        _kb = lv_keyboard_create(_dlg);
        lv_obj_set_size(_kb, W, 140);
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_keyboard_set_mode(_kb, LV_KEYBOARD_MODE_TEXT_UPPER);
        lv_keyboard_set_textarea(_kb, _nameTa);

        lv_obj_add_event_cb(_nameTa, _onTaFocus, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(_nameTa, _onTaFocus, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(_secretTa, _onTaFocus, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(_secretTa, _onTaFocus, LV_EVENT_CLICKED, this);
    }

    static void _onTaFocus(lv_event_t* e) {
        TotpScreen* s = (TotpScreen*)lv_event_get_user_data(e);
        if (!s->_kb) return;
        lv_keyboard_set_textarea(s->_kb, lv_event_get_target(e));
    }

    void _styleTa(lv_obj_t* ta) {
        lv_obj_set_style_bg_color(ta, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_text_color(ta, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(ta, gFontSmall, LV_PART_MAIN);
        lv_obj_set_style_border_width(ta, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(ta, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_all(ta, 4, LV_PART_MAIN);
    }

    void _closeAddDialog() {
        if (!_dlg) return;
        lv_obj_del(_dlg);
        _dlg = nullptr;
        _nameTa = nullptr;
        _secretTa = nullptr;
        _kb = nullptr;
    }

    void _confirmAdd() {
        if (!_nameTa || !_secretTa) return;
        String name = lv_textarea_get_text(_nameTa);
        String sec  = lv_textarea_get_text(_secretTa);
        name.trim(); sec.trim();
        if (!name.length()) {
            ToastManager::getInstance().showToast("Name is empty", ToastType::ALERT);
            return;
        }
        if (name.indexOf('|') >= 0) {
            ToastManager::getInstance().showToast("Name cannot contain |", ToastType::ALERT);
            return;
        }
        uint8_t tmp[64];
        size_t n = totp::base32Decode(sec.c_str(), tmp, sizeof(tmp));
        if (n == 0) {
            ToastManager::getInstance().showToast("Invalid Base32 secret", ToastType::ALERT);
            return;
        }
        if (!_addAccountInternal(name, sec)) {
            ToastManager::getInstance().showToast("Add failed", ToastType::ALERT);
            return;
        }
        _saveAccounts();
        ToastManager::getInstance().showToast("Account added", ToastType::SUCCESS);
        _closeAddDialog();
        _buildList();
        _refreshAll();
    }

    void _openDeleteDialog(int idx) {
        if (idx < 0 || idx >= (int)_accounts.size()) return;
        _delIndex = idx;
        if (_delDlg) { lv_obj_del(_delDlg); _delDlg = nullptr; }

        _delDlg = lv_obj_create(_screen);
        lv_obj_set_size(_delDlg, W, H);
        lv_obj_set_pos(_delDlg, 0, 0);
        lv_obj_set_style_bg_color(_delDlg, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_delDlg, LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_border_width(_delDlg, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_delDlg, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_delDlg, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_delDlg, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* card = lv_obj_create(_delDlg);
        lv_obj_set_size(card, 220, 130);
        lv_obj_center(card);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(card);
        lv_label_set_text(title, "Delete account?");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

        lv_obj_t* name = lv_label_create(card);
        lv_label_set_text(name, _accounts[idx].name.c_str());
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name, 196);
        lv_obj_set_style_text_color(name, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(name, gFontSmall, LV_PART_MAIN);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 28);

        lv_obj_t* cancel = lv_btn_create(card);
        lv_obj_set_size(cancel, 90, 32);
        lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(cancel, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_radius(cancel, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cancel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cancel, 0, LV_PART_MAIN);
        lv_obj_clear_flag(cancel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* cLbl = lv_label_create(cancel);
        lv_label_set_text(cLbl, "Cancel");
        lv_obj_set_style_text_color(cLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(cLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(cLbl);
        lv_obj_add_event_cb(cancel, [](lv_event_t* e) {
            ((TotpScreen*)lv_event_get_user_data(e))->_closeDeleteDialog();
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* ok = lv_btn_create(card);
        lv_obj_set_size(ok, 90, 32);
        lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(ok, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ok, lv_palette_darken(LV_PALETTE_RED, 2), LV_STATE_PRESSED);
        lv_obj_set_style_radius(ok, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ok, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(ok, 0, LV_PART_MAIN);
        lv_obj_clear_flag(ok, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* oLbl = lv_label_create(ok);
        lv_label_set_text(oLbl, "Delete");
        lv_obj_set_style_text_color(oLbl, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(oLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(oLbl);
        lv_obj_add_event_cb(ok, [](lv_event_t* e) {
            ((TotpScreen*)lv_event_get_user_data(e))->_confirmDelete();
        }, LV_EVENT_CLICKED, this);
    }

    void _closeDeleteDialog() {
        if (_delDlg) { lv_obj_del(_delDlg); _delDlg = nullptr; }
        _delIndex = -1;
    }

    void _confirmDelete() {
        if (_delIndex < 0 || _delIndex >= (int)_accounts.size()) {
            _closeDeleteDialog();
            return;
        }
        _accounts.erase(_accounts.begin() + _delIndex);
        _saveAccounts();
        _closeDeleteDialog();
        _buildList();
        _refreshAll();
        ToastManager::getInstance().showToast("Account deleted", ToastType::SUCCESS);
    }
};
