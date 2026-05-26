#include "lua_engine.h"
#include "../ui/theme/theme.h"
#include "../ui/toast/toast_manager.h"
#include "esp_heap_caps.h"

static const char* kEngineKey = "_pickle_engine";

LuaEngine* LuaEngine::fromState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, kEngineKey);
    LuaEngine* eng = (LuaEngine*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return eng;
}

struct WidgetEntry {
    lv_obj_t* obj;
    int tapCbRef;
};

static std::vector<WidgetEntry>* widgetRegistry(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_pickle_widgets");
    auto* v = (std::vector<WidgetEntry>*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return v;
}

static int registerWidget(lua_State* L, lv_obj_t* obj) {
    auto* reg = widgetRegistry(L);
    reg->push_back({ obj, LUA_NOREF });
    return (int)reg->size();
}

static lv_obj_t* widgetAt(lua_State* L, int handle) {
    auto* reg = widgetRegistry(L);
    if (handle < 1 || handle > (int)reg->size()) return nullptr;
    return (*reg)[handle - 1].obj;
}

static lv_color_t parseColor(lua_State* L, int idx, lv_color_t fallback) {
    if (lua_isnumber(L, idx)) {
        return lv_color_hex((uint32_t)lua_tointeger(L, idx));
    }
    if (lua_isstring(L, idx)) {
        const char* s = lua_tostring(L, idx);
        if (s && s[0] == '#' && strlen(s) >= 7) {
            return lv_color_hex(strtoul(s + 1, nullptr, 16));
        }
    }
    return fallback;
}

static const lv_font_t* parseFont(const char* size) {
    if (!size) return gFontNormal;
    if (!strcmp(size, "small")) return gFontSmall;
    if (!strcmp(size, "large")) return gFontLarge;
    return gFontNormal;
}

// lvgl widget-oriented bindings.
static int l_lvgl_create_button(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    lv_color_t color = parseColor(L, 5, gTheme->primary);
    lv_obj_t* b = lv_btn_create(e->host());
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, color, LV_PART_MAIN);
    lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lua_pushinteger(L, registerWidget(L, b));
    return 1;
}

// Lightweight rectangle: a flat lv_obj with no border, shadow, padding or
// scrolling. Intended for bulk-rendered primitives such as raycaster columns
// where a full lv_canvas buffer would not fit in DRAM.
static int l_lvgl_create_rect(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    lv_color_t color = parseColor(L, 5, gTheme->primary);
    lv_obj_t* o = lv_obj_create(e->host());
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lua_pushinteger(L, registerWidget(L, o));
    return 1;
}

static int l_lvgl_create_label(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    const char* text = luaL_checkstring(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    const char* sz = luaL_optstring(L, 4, "normal");
    lv_obj_t* l = lv_label_create(e->host());
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_color(l, gTheme->textDark, LV_PART_MAIN);
    lv_obj_set_style_text_font(l, parseFont(sz), LV_PART_MAIN);
    lua_pushinteger(L, registerWidget(L, l));
    return 1;
}

static int l_lvgl_set_text(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    const char* text = luaL_checkstring(L, 2);
    lv_obj_t* o = widgetAt(L, handle);
    if (!o) return 0;
    // Caller must pass a label handle. Buttons have no text content.
    lv_label_set_text(o, text);
    return 0;
}

static int l_lvgl_set_pos(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    int x = (int)luaL_checkinteger(L, 2);
    int y = (int)luaL_checkinteger(L, 3);
    lv_obj_t* o = widgetAt(L, handle);
    if (o) lv_obj_set_pos(o, x, y);
    return 0;
}

static int l_lvgl_set_size(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    int w = (int)luaL_checkinteger(L, 2);
    int h = (int)luaL_checkinteger(L, 3);
    lv_obj_t* o = widgetAt(L, handle);
    if (o) lv_obj_set_size(o, w, h);
    return 0;
}

static int l_lvgl_set_color(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    lv_color_t color = parseColor(L, 2, gTheme->primary);
    lv_obj_t* o = widgetAt(L, handle);
    if (!o) return 0;
    lv_obj_set_style_bg_color(o, color, LV_PART_MAIN);
    return 0;
}

static int l_lvgl_set_text_color(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    lv_color_t color = parseColor(L, 2, gTheme->textDark);
    lv_obj_t* o = widgetAt(L, handle);
    if (!o) return 0;
    lv_obj_set_style_text_color(o, color, LV_PART_MAIN);
    return 0;
}

static int l_lvgl_set_radius(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    int r = (int)luaL_checkinteger(L, 2);
    lv_obj_t* o = widgetAt(L, handle);
    if (o) lv_obj_set_style_radius(o, r, LV_PART_MAIN);
    return 0;
}

static int l_lvgl_destroy(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    auto* reg = widgetRegistry(L);
    if (handle < 1 || handle > (int)reg->size()) return 0;
    WidgetEntry& w = (*reg)[handle - 1];
    if (w.tapCbRef != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, w.tapCbRef);
        w.tapCbRef = LUA_NOREF;
    }
    if (w.obj) {
        lv_obj_del(w.obj);
        w.obj = nullptr;
    }
    return 0;
}

// The function is invoked with no arguments when the widget is pressed.
static void widgetTapTrampoline(lv_event_t* e) {
    lua_State* L = (lua_State*)lv_event_get_user_data(e);
    if (!L) return;
    int* refPtr = (int*)lv_obj_get_user_data(lv_event_get_target(e));
    if (!refPtr) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, *refPtr);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        Serial.printf("[lua] on_tap error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static int l_lvgl_on_tap(lua_State* L) {
    int handle = (int)luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    auto* reg = widgetRegistry(L);
    if (handle < 1 || handle > (int)reg->size()) return 0;
    WidgetEntry& w = (*reg)[handle - 1];
    if (!w.obj) return 0;
    // Drop any previous reference so re-binding does not leak.
    if (w.tapCbRef != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, w.tapCbRef);
    lua_pushvalue(L, 2);
    w.tapCbRef = luaL_ref(L, LUA_REGISTRYINDEX);
    // Register the LVGL listener and the delete-cleanup only on the first
    // bind; subsequent calls just update the existing slot so re-binding
    // does not stack duplicate handlers.
    int* slot = (int*)lv_obj_get_user_data(w.obj);
    if (!slot) {
        slot = (int*)malloc(sizeof(int));
        lv_obj_set_user_data(w.obj, slot);
        lv_obj_add_event_cb(w.obj, [](lv_event_t* e) {
            int* p = (int*)lv_obj_get_user_data(lv_event_get_target(e));
            if (p) free(p);
        }, LV_EVENT_DELETE, nullptr);
        lv_obj_add_event_cb(w.obj, widgetTapTrampoline, LV_EVENT_CLICKED, L);
    }
    *slot = w.tapCbRef;
    return 0;
}

// canvas low-level drawing on an offscreen buffer placed in the host.
struct CanvasState {
    lv_obj_t* obj;
    lv_color_t* buf;
    int w;
    int h;
};

static CanvasState* canvasState(lua_State* L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_pickle_canvas");
    auto* c = (CanvasState*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return c;
}

static int l_canvas_create(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    CanvasState* c = canvasState(L);
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    if (c->buf) { heap_caps_free(c->buf); c->buf = nullptr; }
    if (c->obj) { lv_obj_del(c->obj); c->obj = nullptr; }
    c->w = w;
    c->h = h;
    size_t bytes = (size_t)w * (size_t)h * sizeof(lv_color_t);
    // Prefer PSRAM when present, fall back to any 8-bit accessible heap.
    c->buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!c->buf) {
        c->buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
    }
    if (!c->buf) {
        luaL_error(L, "canvas alloc failed (%d bytes)", (int)bytes);
        return 0;
    }
    c->obj = lv_canvas_create(e->host());
    lv_canvas_set_buffer(c->obj, c->buf, w, h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(c->obj, x, y);
    return 0;
}

static int l_canvas_clear(lua_State* L) {
    CanvasState* c = canvasState(L);
    if (!c->obj) return 0;
    lv_color_t color = parseColor(L, 1, gTheme->background);
    lv_canvas_fill_bg(c->obj, color, LV_OPA_COVER);
    return 0;
}

static int l_canvas_draw_rect(lua_State* L) {
    CanvasState* c = canvasState(L);
    if (!c->obj) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    lv_color_t color = parseColor(L, 5, gTheme->primary);
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.border_width = 0;
    lv_canvas_draw_rect(c->obj, x, y, w, h, &dsc);
    return 0;
}

static int l_canvas_draw_text(lua_State* L) {
    CanvasState* c = canvasState(L);
    if (!c->obj) return 0;
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    const char* text = luaL_checkstring(L, 3);
    lv_color_t color = parseColor(L, 4, gTheme->textDark);
    const char* sz = luaL_optstring(L, 5, "normal");
    lv_draw_label_dsc_t dsc;
    lv_draw_label_dsc_init(&dsc);
    dsc.color = color;
    dsc.font = parseFont(sz);
    lv_canvas_draw_text(c->obj, x, y, c->w - x, &dsc, text);
    return 0;
}

// game control flow used by every script.
static int l_game_set_hud(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    const char* l = luaL_optstring(L, 1, "");
    const char* r = luaL_optstring(L, 2, "");
    if (e) e->invokeHud(l, r);
    return 0;
}

static int l_game_set_score(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    e->setScore((int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_game_get_score(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    lua_pushinteger(L, e->score());
    return 1;
}

static int l_game_add_score(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    e->addScore((int)luaL_checkinteger(L, 1));
    return 0;
}

// Triggers the standard game-over modal with the current score.
static int l_game_end(lua_State* L) {
    LuaEngine* e = LuaEngine::fromState(L);
    e->triggerGameOver();
    return 0;
}

// TimerEntry survives until the engine is destroyed. The auto-destruction
// path for one-shot timers only flips `alive` to false and unrefs the Lua
// callback, but never frees the entry. The engine destructor walks the
// tracked entries, deletes any still-live lv_timer and frees the entries.
// This avoids dangling lv_timer_t pointers when many one-shot timers were
// scheduled during gameplay.
struct TimerEntry {
    int cbRef;
    bool repeat;
    bool alive;
    lv_timer_t* timer;
    lua_State* L;
};

static void timerTrampoline(lv_timer_t* t) {
    TimerEntry* e = (TimerEntry*)t->user_data;
    if (!e || !e->alive) return;
    lua_State* L = e->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, e->cbRef);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        Serial.printf("[lua] timer error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    if (!e->repeat) {
        e->alive = false;
        luaL_unref(L, LUA_REGISTRYINDEX, e->cbRef);
        e->cbRef = LUA_NOREF;
        lv_timer_del(t);
        e->timer = nullptr;
    }
}

static int l_game_timer(lua_State* L) {
    LuaEngine* eng = LuaEngine::fromState(L);
    int ms = (int)luaL_checkinteger(L, 1);
    bool repeat = lua_toboolean(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    auto* entry = (TimerEntry*)malloc(sizeof(TimerEntry));
    entry->cbRef = ref;
    entry->repeat = repeat;
    entry->alive = true;
    entry->L = L;
    entry->timer = lv_timer_create(timerTrampoline, ms, entry);
    if (!repeat) lv_timer_set_repeat_count(entry->timer, 1);
    eng->trackTimerEntry(entry);
    return 0;
}

// kind is one of: "success", "alert", "info".
static int l_game_toast(lua_State* L) {
    const char* text = luaL_checkstring(L, 1);
    const char* kind = luaL_optstring(L, 2, "info");
    ToastType t = ToastType::INFO;
    if (!strcmp(kind, "success")) t = ToastType::SUCCESS;
    else if (!strcmp(kind, "alert")) t = ToastType::ALERT;
    ToastManager::getInstance().showToast(text, t);
    return 0;
}

static int l_game_random(lua_State* L) {
    int a = (int)luaL_checkinteger(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    lua_pushinteger(L, random(a, b + 1));
    return 1;
}

// Milliseconds since boot, exposed so scripts can measure elapsed time
// without spinning a high-frequency tick timer.
static int l_game_now(lua_State* L) {
    lua_pushinteger(L, (int)millis());
    return 1;
}

// Allowed names: primary, primary_dark, primary_light, secondary,
// secondary_dark, background, background_popup, text_dark, text_soft,
// success, error, alert.
static int l_game_theme(lua_State* L) {
    const char* n = luaL_checkstring(L, 1);
    lv_color_t c = gTheme->primary;
    if (!strcmp(n, "primary")) c = gTheme->primary;
    else if (!strcmp(n, "primary_dark")) c = gTheme->primaryDark;
    else if (!strcmp(n, "primary_light")) c = gTheme->primaryLight;
    else if (!strcmp(n, "secondary")) c = gTheme->secondary;
    else if (!strcmp(n, "secondary_dark")) c = gTheme->secondaryDark;
    else if (!strcmp(n, "background")) c = gTheme->background;
    else if (!strcmp(n, "background_popup")) c = gTheme->backgroundPopup;
    else if (!strcmp(n, "text_dark")) c = gTheme->textDark;
    else if (!strcmp(n, "text_soft")) c = gTheme->textSoft;
    else if (!strcmp(n, "success")) c = gTheme->successBg;
    else if (!strcmp(n, "error")) c = gTheme->errorBg;
    else if (!strcmp(n, "alert")) c = gTheme->alertBg;
    lua_pushinteger(L, lv_color_to32(c) & 0xFFFFFF);
    return 1;
}

LuaEngine::LuaEngine(lv_obj_t* host, HudCb hud, GameOverCb gameOver)
    : _host(host), _hud(hud), _gameOver(gameOver) {}

LuaEngine::~LuaEngine() {
    _stopTimers();
    if (_state) {
        lua_getfield(_state, LUA_REGISTRYINDEX, "_pickle_canvas");
        auto* c = (CanvasState*)lua_touserdata(_state, -1);
        lua_pop(_state, 1);
        if (c) {
            if (c->buf) heap_caps_free(c->buf);
            delete c;
        }
        auto* reg = widgetRegistry(_state);
        if (reg) delete reg;
        lua_close(_state);
        _state = nullptr;
    }
}

void LuaEngine::_stopTimers() {
    for (void* p : _timers) {
        TimerEntry* e = (TimerEntry*)p;
        if (!e) continue;
        if (e->alive) {
            if (e->cbRef != LUA_NOREF && _state) {
                luaL_unref(_state, LUA_REGISTRYINDEX, e->cbRef);
                e->cbRef = LUA_NOREF;
            }
            if (e->timer) {
                lv_timer_del(e->timer);
                e->timer = nullptr;
            }
            e->alive = false;
        }
        free(e);
    }
    _timers.clear();
}

void LuaEngine::triggerGameOver() {
    // Reentrancy guard: timers stay alive after game.finish() until the
    // screen is destroyed, so a tick that fires again must not show a second
    // game-over modal.
    if (_gameOverFired) return;
    _gameOverFired = true;
    // Do NOT stop timers here: game.finish() is usually invoked from inside a
    // timer callback. Tearing the entry down while its trampoline is on the
    // call stack would be a use-after-free. Timers are cleaned by the
    // destructor when the host screen is destroyed.
    if (_gameOver) _gameOver(_score);
}

bool LuaEngine::run(const String& script) {
    _state = luaL_newstate();
    if (!_state) return false;
    luaL_openlibs(_state);
    lua_pushlightuserdata(_state, this);
    lua_setfield(_state, LUA_REGISTRYINDEX, kEngineKey);
    auto* wreg = new std::vector<WidgetEntry>();
    lua_pushlightuserdata(_state, wreg);
    lua_setfield(_state, LUA_REGISTRYINDEX, "_pickle_widgets");
    auto* cstate = new CanvasState{ nullptr, nullptr, 0, 0 };
    lua_pushlightuserdata(_state, cstate);
    lua_setfield(_state, LUA_REGISTRYINDEX, "_pickle_canvas");
    _registerApi();
    if (luaL_loadstring(_state, script.c_str()) != LUA_OK) {
        Serial.printf("[lua] load error: %s\n", lua_tostring(_state, -1));
        ToastManager::getInstance().showToast("Lua load error", ToastType::ALERT);
        lua_pop(_state, 1);
        return false;
    }
    if (lua_pcall(_state, 0, 0, 0) != LUA_OK) {
        Serial.printf("[lua] runtime error: %s\n", lua_tostring(_state, -1));
        ToastManager::getInstance().showToast("Lua runtime error", ToastType::ALERT);
        lua_pop(_state, 1);
        return false;
    }
    return true;
}

void LuaEngine::_registerApi() {
    static const luaL_Reg lvglApi[] = {
        { "create_button", l_lvgl_create_button },
        { "create_rect", l_lvgl_create_rect },
        { "create_label", l_lvgl_create_label },
        { "set_text", l_lvgl_set_text },
        { "set_pos", l_lvgl_set_pos },
        { "set_size", l_lvgl_set_size },
        { "set_color", l_lvgl_set_color },
        { "set_text_color", l_lvgl_set_text_color },
        { "set_radius", l_lvgl_set_radius },
        { "destroy", l_lvgl_destroy },
        { "on_tap", l_lvgl_on_tap },
        { nullptr, nullptr }
    };
    luaL_newlib(_state, lvglApi);
    lua_setglobal(_state, "lvgl");
    static const luaL_Reg canvasApi[] = {
        { "create", l_canvas_create },
        { "clear", l_canvas_clear },
        { "draw_rect", l_canvas_draw_rect },
        { "draw_text", l_canvas_draw_text },
        { nullptr, nullptr }
    };
    luaL_newlib(_state, canvasApi);
    lua_setglobal(_state, "canvas");
    static const luaL_Reg gameApi[] = {
        { "set_hud", l_game_set_hud },
        { "set_score", l_game_set_score },
        { "score", l_game_get_score },
        { "add_score", l_game_add_score },
        { "finish", l_game_end },
        { "timer", l_game_timer },
        { "toast", l_game_toast },
        { "random", l_game_random },
        { "now", l_game_now },
        { "theme", l_game_theme },
        { nullptr, nullptr }
    };
    luaL_newlib(_state, gameApi);
    lua_setglobal(_state, "game");
    lua_pushinteger(_state, 240);
    lua_setglobal(_state, "SCREEN_W");
    lua_pushinteger(_state, 320 - 36);
    lua_setglobal(_state, "SCREEN_H");
}
