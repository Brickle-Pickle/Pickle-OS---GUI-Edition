#pragma once
#include <lvgl.h>
#include <ArduinoJson.h>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "../../network/wifi_manager.h"
#include "../../network/game_api.h"
#include "../../storage/game_installer.h"

// GameStoreScreen - browse the remote catalog and install games to SD.
//
// Networking runs in a dedicated FreeRTOS task; the LVGL task is only
// touched via a polling lv_timer that watches a shared atomic flag.
class GameStoreScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildBody();
        _showStatus("Loading catalog...");
        _startCatalogFetch();
    }

    void onDestroy() override {
        _cancelTimer();
        // Tell any in-flight worker that the screen is gone; the worker frees
        // the shared block on its own when it finishes.
        if (_shared) {
            _shared->screen_alive = false;
            _shared = nullptr;
        }
        if (_screen) { lv_obj_del(_screen); _screen = nullptr; }
    }

private:
    // Shared block lives on the heap so it survives the screen if the user
    // navigates away mid-fetch; the worker task frees it on exit.
    struct Shared {
        volatile bool job_done = false;
        volatile bool screen_alive = true;
        String result;
        // op zero is catalog, op one is install.
        int op = 0;
        String installId;
        String installResult;
        bool installOk = false;
    };
    Shared* _shared = nullptr;
    lv_obj_t* _list = nullptr;
    lv_obj_t* _statusLbl = nullptr;
    lv_timer_t* _pollTmr = nullptr;

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

        lv_obj_t* title = lv_label_create(header);
        lv_label_set_text(title, "Game Store");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        lv_obj_t* refreshBtn = lv_btn_create(header);
        lv_obj_set_size(refreshBtn, 32, 28);
        lv_obj_align(refreshBtn, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_set_style_bg_color(refreshBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(refreshBtn, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(refreshBtn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(refreshBtn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(refreshBtn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* rIco = lv_label_create(refreshBtn);
        lv_label_set_text(rIco, LV_SYMBOL_REFRESH);
        lv_obj_set_style_text_color(rIco, gTheme->textDark, LV_PART_MAIN);
        lv_obj_center(rIco);
        lv_obj_add_event_cb(refreshBtn, [](lv_event_t* e) {
            GameStoreScreen* s = (GameStoreScreen*)lv_event_get_user_data(e);
            s->_clearList();
            s->_showStatus("Reloading...");
            s->_startCatalogFetch();
        }, LV_EVENT_CLICKED, this);
    }

    void _buildBody() {
        _list = lv_obj_create(_screen);
        lv_obj_set_size(_list, 240, 320 - 36);
        lv_obj_set_pos(_list, 0, 36);
        lv_obj_set_style_bg_color(_list, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_list, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(_list, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }

    void _clearList() { lv_obj_clean(_list); _statusLbl = nullptr; }

    void _showStatus(const char* msg) {
        if (!_statusLbl) {
            _statusLbl = lv_label_create(_list);
            lv_obj_set_style_text_color(_statusLbl, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(_statusLbl, gFontSmall, LV_PART_MAIN);
        }
        lv_label_set_text(_statusLbl, msg);
    }

    void _cancelTimer() {
        if (_pollTmr) { lv_timer_del(_pollTmr); _pollTmr = nullptr; }
    }

    static void _workerTask(void* arg) {
        Shared* sh = (Shared*)arg;
        if (sh->op == 0) {
            sh->result = GameApi::getInstance().fetchCatalog();
        } else {
            // Install downloads the manifest, then the entry script declared
            // in the manifest (defaults to "game.lua").
            String manifest = GameApi::getInstance().fetchManifest(sh->installId);
            if (!manifest.length()) {
                sh->installOk = false;
                sh->installResult = "Manifest download failed";
            } else {
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, manifest);
                if (err) {
                    sh->installOk = false;
                    sh->installResult = "Bad manifest JSON";
                } else {
                    String entry = String((const char*)(doc["entry"] | "game.lua"));
                    String script = GameApi::getInstance().fetchAsset(sh->installId, entry);
                    if (!script.length()) {
                        sh->installOk = false;
                        sh->installResult = "Script download failed";
                    } else {
                        bool ok = GameInstaller::getInstance().writeManifest(sh->installId, manifest)
                               && GameInstaller::getInstance().writeAsset(sh->installId, entry, script);
                        sh->installOk = ok;
                        sh->installResult = ok ? "Installed" : "Write failed";
                    }
                }
            }
        }
        sh->job_done = true;
        // The screen may be gone; nobody else owns the block.
        if (!sh->screen_alive) delete sh;
        vTaskDelete(nullptr);
    }

    void _startCatalogFetch() {
        if (!WifiManager::getInstance().connected()) {
            _clearList();
            _showStatus("WiFi not connected.\nEnable WiFi in Settings.");
            return;
        }
        if (_shared && !_shared->job_done) return; // already running
        if (_shared) { delete _shared; _shared = nullptr; }
        _shared = new Shared();
        _shared->op = 0;
        xTaskCreatePinnedToCore(_workerTask, "store-fetch", 12288, _shared, 1, nullptr, 0);
        _cancelTimer();
        _pollTmr = lv_timer_create([](lv_timer_t* t) {
            ((GameStoreScreen*)t->user_data)->_pollCatalog();
        }, 100, this);
    }

    void _pollCatalog() {
        if (!_shared || !_shared->job_done) return;
        _cancelTimer();

        if (!_shared->result.length()) {
            _clearList();
            _showStatus("Failed to reach server.");
            delete _shared; _shared = nullptr;
            return;
        }
        _renderCatalog(_shared->result);
        delete _shared; _shared = nullptr;
    }

    void _renderCatalog(const String& json) {
        _clearList();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, json);
        if (err) {
            _showStatus("Bad JSON from server.");
            return;
        }
        JsonArray games = doc["games"].as<JsonArray>();
        if (games.size() == 0) {
            _showStatus("No games available.");
            return;
        }
        for (JsonObject g : games) {
            _addCard(g);
        }
    }

    static const char* _iconFor(const char* type) {
        if (!type) return LV_SYMBOL_PLAY;
        if (!strcmp(type, "tap")) return LV_SYMBOL_PLAY;
        if (!strcmp(type, "quiz")) return LV_SYMBOL_LIST;
        if (!strcmp(type, "reaction")) return LV_SYMBOL_EYE_OPEN;
        if (!strcmp(type, "guess")) return LV_SYMBOL_OK;
        return LV_SYMBOL_PLAY;
    }

    void _addCard(JsonObject g) {
        String id = String((const char*)(g["id"] | ""));
        String name = String((const char*)(g["name"] | ""));
        String desc = String((const char*)(g["description"] | ""));
        String type = String((const char*)(g["type"] | ""));
        String ver = String((const char*)(g["version"] | ""));
        String author = String((const char*)(g["author"] | ""));
        bool installed = GameInstaller::getInstance().isInstalled(id);

        // Layout: 220x100 card with a colored icon strip on the left, two
        // text rows on top, the description in the middle and the action
        // button anchored bottom-right.
        lv_obj_t* card = lv_obj_create(_list);
        lv_obj_set_size(card, 220, 100);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        // Left icon strip
        lv_obj_t* strip = lv_obj_create(card);
        lv_obj_set_size(strip, 44, 100);
        lv_obj_set_pos(strip, 0, 0);
        lv_obj_set_style_bg_color(strip,
            installed ? gTheme->secondaryDark : gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_border_width(strip, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(strip, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(strip, 0, LV_PART_MAIN);
        lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ico = lv_label_create(strip);
        lv_label_set_text(ico, _iconFor(type.c_str()));
        lv_obj_set_style_text_color(ico, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(ico, gFontIconLarge, LV_PART_MAIN);
        lv_obj_center(ico);

        // Title row, truncated so a long name does not push the version off
        lv_obj_t* nm = lv_label_create(card);
        lv_obj_set_size(nm, 130, gFontNormal->line_height);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
        lv_label_set_text(nm, name.c_str());
        lv_obj_set_style_text_color(nm, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(nm, gFontNormal, LV_PART_MAIN);
        lv_obj_set_pos(nm, 54, 8);

        lv_obj_t* ver_lbl = lv_label_create(card);
        lv_label_set_text_fmt(ver_lbl, "v%s", ver.c_str());
        lv_obj_set_style_text_color(ver_lbl, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(ver_lbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(ver_lbl, LV_ALIGN_TOP_RIGHT, -10, 10);

        // Author line, single-line truncated
        lv_obj_t* by = lv_label_create(card);
        lv_obj_set_size(by, 156, gFontSmall->line_height);
        lv_label_set_long_mode(by, LV_LABEL_LONG_DOT);
        lv_label_set_text_fmt(by, "by %s", author.length() ? author.c_str() : "unknown");
        lv_obj_set_style_text_color(by, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(by, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(by, 54, 8 + gFontNormal->line_height + 2);

        // Description, single-line truncated
        lv_obj_t* d = lv_label_create(card);
        lv_obj_set_size(d, 156, gFontSmall->line_height);
        lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
        lv_label_set_text(d, desc.c_str());
        lv_obj_set_style_text_color(d, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(d, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(d, 54,
                       8 + gFontNormal->line_height + 2 + gFontSmall->line_height + 4);

        // Action button anchored bottom-right
        lv_obj_t* btn = lv_btn_create(card);
        lv_obj_set_size(btn, 96, 26);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
        lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(btn,
            installed ? gTheme->successBg : gTheme->secondary, LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn,
            installed ? gTheme->successText : gTheme->secondaryDark, LV_STATE_PRESSED);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl,
            installed ? (LV_SYMBOL_OK " Installed")
                      : (LV_SYMBOL_DOWNLOAD " Install"));
        lv_obj_set_style_text_color(lbl,
            installed ? gTheme->successText : gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(lbl);

        // Stash id in user_data so the callback knows what to install.
        char* idHeap = strdup(id.c_str());
        lv_obj_set_user_data(btn, idHeap);
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            GameStoreScreen* s = (GameStoreScreen*)lv_event_get_user_data(e);
            const char* gid = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
            if (!gid) return;
            String idStr(gid);
            if (GameInstaller::getInstance().isInstalled(idStr)) {
                ToastManager::getInstance().showToast("Already installed", ToastType::SUCCESS);
                return;
            }
            s->_startInstall(idStr);
        }, LV_EVENT_CLICKED, this);
        // Free the heap-stashed id with the button.
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            char* p = (char*)lv_obj_get_user_data(lv_event_get_target(e));
            if (p) free(p);
        }, LV_EVENT_DELETE, nullptr);
    }

    void _startInstall(const String& id) {
        if (!WifiManager::getInstance().connected()) {
            ToastManager::getInstance().showToast("WiFi disconnected", ToastType::ALERT);
            return;
        }
        if (_shared && !_shared->job_done) {
            ToastManager::getInstance().showToast("Busy...", ToastType::ALERT);
            return;
        }
        if (_shared) { delete _shared; _shared = nullptr; }
        _shared = new Shared();
        _shared->op = 1;
        _shared->installId = id;
        ToastManager::getInstance().showToast((String("Downloading ") + id + "...").c_str(),
                                              ToastType::SUCCESS);
        xTaskCreatePinnedToCore(_workerTask, "store-install", 8192, _shared, 1, nullptr, 0);
        _cancelTimer();
        _pollTmr = lv_timer_create([](lv_timer_t* t) {
            ((GameStoreScreen*)t->user_data)->_pollInstall();
        }, 100, this);
    }

    void _pollInstall() {
        if (!_shared || !_shared->job_done) return;
        _cancelTimer();
        ToastManager::getInstance().showToast(_shared->installResult.c_str(),
            _shared->installOk ? ToastType::SUCCESS : ToastType::ALERT);
        delete _shared; _shared = nullptr;
        // Re-render so the button flips to "Installed".
        _clearList();
        _showStatus("Reloading...");
        _startCatalogFetch();
    }
};
