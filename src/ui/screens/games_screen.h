#pragma once
#include <ArduinoJson.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "games/tap_game_screen.h"
#include "games/memory_game_screen.h"
#include "games/lua_game_screen.h"
#include "game_store_screen.h"
#include "../../storage/game_installer.h"

// GamesScreen - Lists the built-in games plus any game installed under
// /pickle-os/games-hub/ and exposes the entry point to the remote game store.
class GamesScreen : public ScreenBase {
public:
    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        _buildHeader();
        _buildContent();
        _buildStoreButton();
    }

    void onResume() override {
        // Rebuild the list so newly installed games appear after returning
        // from the store.
        if (!_screen) return;
        if (_list) { lv_obj_del(_list); _list = nullptr; }
        _buildContent();
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
        _list = nullptr;
    }

private:
    lv_obj_t* _list = nullptr;

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
        lv_label_set_text(title, "Games");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontNormal, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);
    }

    void _buildContent() {
        // Scrollable list so the count of installed games is unbounded.
        _list = lv_obj_create(_screen);
        lv_obj_set_size(_list, 240, 320 - 36 - 56);
        lv_obj_set_pos(_list, 0, 36);
        lv_obj_set_style_bg_color(_list, gTheme->background, LV_PART_MAIN);
        lv_obj_set_style_border_width(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_list, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_list, 8, LV_PART_MAIN);
        lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(_list, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Built-in titles (always available, shipped in the firmware).
        _addBuiltinCard(LV_SYMBOL_PLAY, "Tap Frenzy", "Tap targets before they vanish",
            [](lv_event_t*) {
                ScreenManager::getInstance().navigateTo(
                    new TapGameScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
            });
        _addBuiltinCard(LV_SYMBOL_REFRESH, "Memory", "Repeat the color sequence",
            [](lv_event_t*) {
                ScreenManager::getInstance().navigateTo(
                    new MemoryGameScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
            });

        // Downloaded games: each has its own manifest.json on the SD card.
        auto ids = GameInstaller::getInstance().listInstalled();
        for (auto& id : ids) {
            String manifest = GameInstaller::getInstance().readManifest(id);
            JsonDocument doc;
            if (deserializeJson(doc, manifest)) continue;
            String name = String((const char*)(doc["name"] | id.c_str()));
            String desc = String((const char*)(doc["description"] | ""));
            const char* iconName = doc["icon"] | "play";
            _addDownloadedCard(_iconFor(iconName), name, desc, id);
        }

        if (ids.empty()) {
            lv_obj_t* hint = lv_label_create(_list);
            lv_label_set_text(hint, "No downloaded games yet.\nVisit the store below.");
            lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
            lv_obj_set_width(hint, 200);
            lv_obj_set_style_text_color(hint, gTheme->textSoft, LV_PART_MAIN);
            lv_obj_set_style_text_font(hint, gFontSmall, LV_PART_MAIN);
            lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        }
    }

    static const char* _iconFor(const char* name) {
        if (!name) return LV_SYMBOL_PLAY;
        if (!strcmp(name, "play")) return LV_SYMBOL_PLAY;
        if (!strcmp(name, "list")) return LV_SYMBOL_LIST;
        if (!strcmp(name, "eye_open")) return LV_SYMBOL_EYE_OPEN;
        if (!strcmp(name, "ok")) return LV_SYMBOL_OK;
        if (!strcmp(name, "bell")) return LV_SYMBOL_BELL;
        if (!strcmp(name, "bullet")) return LV_SYMBOL_BULLET;
        if (!strcmp(name, "home")) return LV_SYMBOL_HOME;
        if (!strcmp(name, "image")) return LV_SYMBOL_IMAGE;
        if (!strcmp(name, "file")) return LV_SYMBOL_FILE;
        if (!strcmp(name, "settings")) return LV_SYMBOL_SETTINGS;
        if (!strcmp(name, "refresh")) return LV_SYMBOL_REFRESH;
        if (!strcmp(name, "download")) return LV_SYMBOL_DOWNLOAD;
        return LV_SYMBOL_PLAY;
    }

    void _addBuiltinCard(const char* icon, const char* name, const char* desc,
                         lv_event_cb_t cb) {
        lv_obj_t* card = _makeCard(icon, name, desc);
        lv_obj_add_event_cb(card, cb, LV_EVENT_CLICKED, nullptr);
    }

    void _addDownloadedCard(const char* icon, const String& name,
                            const String& desc, const String& id) {
        lv_obj_t* card = _makeCard(icon, name.c_str(), desc.c_str(), 128);
        // The id is heap-stashed and freed when the card is destroyed.
        char* idHeap = strdup(id.c_str());
        lv_obj_set_user_data(card, idHeap);
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            const char* gid = (const char*)lv_obj_get_user_data(lv_event_get_target(e));
            if (!gid) return;
            ScreenManager::getInstance().navigateTo(
                new LuaGameScreen(String(gid)), LV_SCR_LOAD_ANIM_MOVE_LEFT);
        }, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(card, [](lv_event_t* e) {
            char* p = (char*)lv_obj_get_user_data(lv_event_get_target(e));
            if (p) free(p);
        }, LV_EVENT_DELETE, nullptr);

        // Trash button anchored to the right edge of the card. It does not
        // bubble its click to the card (LVGL default), so tapping the icon
        // never launches the game.
        lv_obj_t* trash = lv_btn_create(card);
        lv_obj_set_size(trash, 32, 32);
        lv_obj_align(trash, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_style_bg_color(trash, gTheme->errorBg, LV_PART_MAIN);
        lv_obj_set_style_bg_color(trash, gTheme->errorText, LV_STATE_PRESSED);
        lv_obj_set_style_radius(trash, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(trash, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(trash, 0, LV_PART_MAIN);
        lv_obj_clear_flag(trash, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* trashIco = lv_label_create(trash);
        lv_label_set_text(trashIco, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_color(trashIco, gTheme->errorText, LV_PART_MAIN);
        lv_obj_center(trashIco);
        lv_obj_set_user_data(trash, this);
        lv_obj_add_event_cb(trash, [](lv_event_t* e) {
            GamesScreen* s = (GamesScreen*)lv_obj_get_user_data(lv_event_get_target(e));
            lv_obj_t* card = lv_obj_get_parent(lv_event_get_target(e));
            const char* gid = (const char*)lv_obj_get_user_data(card);
            if (s && gid) s->_askUninstall(String(gid));
        }, LV_EVENT_CLICKED, nullptr);
    }

    // Confirmation modal for the uninstall action. The current id is stashed
    // on the message box user_data so the button handler can recover it.
    void _askUninstall(const String& id) {
        String manifest = GameInstaller::getInstance().readManifest(id);
        String name = id;
        if (manifest.length()) {
            JsonDocument doc;
            if (!deserializeJson(doc, manifest)) {
                name = String((const char*)(doc["name"] | id.c_str()));
            }
        }
        static const char* btns[] = { "Cancel", "Uninstall", "" };
        String text = String("Remove ") + name + " from this device?";
        lv_obj_t* box = lv_msgbox_create(NULL, "Uninstall", text.c_str(), btns, false);
        lv_obj_set_style_text_font(box, gFontNormal, LV_PART_MAIN);
        lv_obj_center(box);
        // Bundle the id and the originating screen pointer so the handler
        // can detect whether the GamesScreen is still the active screen
        // before touching it (the user could navigate away with the modal
        // open).
        struct Ctx {
            String id;
            GamesScreen* screen;
        };
        Ctx* ctx = new Ctx{ id, this };
        lv_obj_set_user_data(box, ctx);
        lv_obj_add_event_cb(box, [](lv_event_t* e) {
            lv_obj_t* mb = lv_event_get_current_target(e);
            const char* btn = lv_msgbox_get_active_btn_text(mb);
            Ctx* c = (Ctx*)lv_obj_get_user_data(mb);
            if (btn && c && !strcmp(btn, "Uninstall")) {
                bool ok = GameInstaller::getInstance().uninstall(c->id);
                ToastManager::getInstance().showToast(
                    ok ? "Uninstalled" : "Uninstall failed",
                    ok ? ToastType::SUCCESS : ToastType::ALERT);
                if (ok && ScreenManager::getInstance().currentScreen() == c->screen) {
                    c->screen->_rebuildList();
                }
            }
            lv_msgbox_close(mb);
        }, LV_EVENT_VALUE_CHANGED, nullptr);
        lv_obj_add_event_cb(box, [](lv_event_t* e) {
            Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
            if (c) delete c;
        }, LV_EVENT_DELETE, nullptr);
    }

    // Rebuild the list of games (used after uninstall to drop the removed
    // card without leaving the screen).
    void _rebuildList() {
        if (_list) { lv_obj_del(_list); _list = nullptr; }
        _buildContent();
    }

    lv_obj_t* _makeCard(const char* icon, const char* name, const char* desc,
                        int descWidth = 170) {
        lv_obj_t* card = lv_btn_create(_list);
        lv_obj_set_size(card, 220, 64);
        lv_obj_set_style_bg_color(card, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_bg_color(card, gTheme->primaryDark, LV_STATE_PRESSED);
        lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(card, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ico = lv_label_create(card);
        lv_label_set_text(ico, icon);
        lv_obj_set_style_text_color(ico, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(ico, gFontIconLarge, LV_PART_MAIN);
        lv_obj_align(ico, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* nm = lv_label_create(card);
        lv_label_set_text(nm, name);
        lv_obj_set_style_text_color(nm, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(nm, gFontNormal, LV_PART_MAIN);
        lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 36, 0);

        lv_obj_t* d = lv_label_create(card);
        lv_obj_set_size(d, descWidth, gFontSmall->line_height);
        lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
        lv_label_set_text(d, desc);
        lv_obj_set_style_text_color(d, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(d, gFontSmall, LV_PART_MAIN);
        lv_obj_align(d, LV_ALIGN_BOTTOM_LEFT, 36, 0);
        return card;
    }

    // Store button pinned to the bottom; launches the remote catalog.
    void _buildStoreButton() {
        lv_obj_t* store = lv_btn_create(_screen);
        lv_obj_set_size(store, 220, 44);
        lv_obj_set_pos(store, 10, 320 - 56);
        lv_obj_set_style_bg_color(store, gTheme->secondary, LV_PART_MAIN);
        lv_obj_set_style_bg_color(store, gTheme->secondaryDark, LV_STATE_PRESSED);
        lv_obj_set_style_radius(store, 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(store, 0, LV_PART_MAIN);
        lv_obj_clear_flag(store, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lbl = lv_label_create(store);
        lv_label_set_text(lbl, LV_SYMBOL_DOWNLOAD " Get more games");
        lv_obj_set_style_text_color(lbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontNormal, LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(store, [](lv_event_t*) {
            ScreenManager::getInstance().navigateTo(
                new GameStoreScreen(), LV_SCR_LOAD_ANIM_MOVE_LEFT);
        }, LV_EVENT_CLICKED, nullptr);
    }
};
