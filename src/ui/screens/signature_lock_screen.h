#pragma once
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "home_screen.h"
#include "pin_lock_screen.h"
#include "../../crypto/signature_features.h"
#include "../../crypto/signature_net.h"
#include "../../crypto/signature_store.h"

// SignatureLockScreen - prompts the user to draw their signature inside the
// pad. The captured strokes are turned into a feature vector, embedded by the
// tiny MLP in signature_net.h, and compared against the centroid saved during
// enrolment. Three consecutive failures fall back to the PIN lock screen.
class SignatureLockScreen : public ScreenBase {
public:
    static const int MAX_POINTS = 384;
    static const int MAX_FAILS = 3;
    static const int PAD_W = 220;
    static const int PAD_H = 180;

    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, gTheme->background, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* icon = lv_label_create(_screen);
        lv_label_set_text(icon, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_color(icon, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_text_font(icon, gFontIconLarge, LV_PART_MAIN);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 8);

        lv_obj_t* title = lv_label_create(_screen);
        lv_label_set_text(title, "Sign to unlock");
        lv_obj_set_style_text_color(title, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontLarge, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

        _hint = lv_label_create(_screen);
        lv_label_set_text(_hint, "Draw your signature in the pad");
        lv_obj_set_style_text_color(_hint, gTheme->textSoft, LV_PART_MAIN);
        lv_obj_set_style_text_font(_hint, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_hint, LV_ALIGN_TOP_MID, 0, 64);

        _pad = lv_obj_create(_screen);
        lv_obj_set_size(_pad, PAD_W, PAD_H);
        lv_obj_align(_pad, LV_ALIGN_TOP_MID, 0, 84);
        lv_obj_set_style_bg_color(_pad, gTheme->backgroundPopup, LV_PART_MAIN);
        lv_obj_set_style_border_color(_pad, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_border_width(_pad, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(_pad, 8, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_pad, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_pad, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_pad, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(_pad, _onPadEvent, LV_EVENT_PRESSING, this);
        lv_obj_add_event_cb(_pad, _onPadEvent, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(_pad, _onPadEvent, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(_pad, _onPadDraw,  LV_EVENT_DRAW_MAIN_END, this);

        lv_obj_t* clearBtn = lv_btn_create(_screen);
        lv_obj_set_size(clearBtn, 96, 36);
        lv_obj_align(clearBtn, LV_ALIGN_BOTTOM_LEFT, 8, -8);
        lv_obj_set_style_bg_color(clearBtn, gTheme->primaryDark, LV_PART_MAIN);
        lv_obj_set_style_radius(clearBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(clearBtn, _onClear, LV_EVENT_CLICKED, this);
        lv_obj_t* clrLbl = lv_label_create(clearBtn);
        lv_label_set_text(clrLbl, LV_SYMBOL_TRASH " Clear");
        lv_obj_set_style_text_color(clrLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(clrLbl, gFontIcon, LV_PART_MAIN);
        lv_obj_center(clrLbl);

        lv_obj_t* okBtn = lv_btn_create(_screen);
        lv_obj_set_size(okBtn, 120, 36);
        lv_obj_align(okBtn, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
        lv_obj_set_style_bg_color(okBtn, gTheme->primary, LV_PART_MAIN);
        lv_obj_set_style_radius(okBtn, 8, LV_PART_MAIN);
        lv_obj_add_event_cb(okBtn, _onUnlock, LV_EVENT_CLICKED, this);
        lv_obj_t* okLbl = lv_label_create(okBtn);
        lv_label_set_text(okLbl, LV_SYMBOL_OK " Unlock");
        lv_obj_set_style_text_color(okLbl, gTheme->textDark, LV_PART_MAIN);
        lv_obj_set_style_text_font(okLbl, gFontIcon, LV_PART_MAIN);
        lv_obj_center(okLbl);
    }

    void onDestroy() override {
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    lv_obj_t* _pad = nullptr;
    lv_obj_t* _hint = nullptr;
    SignatureFeatures::Point _pts[MAX_POINTS];
    int _ptCount = 0;
    uint8_t _currentStroke = 0;
    uint32_t _t0 = 0;
    int _failCount = 0;
    bool _strokeOpen = false;

    void _resetPad() {
        _ptCount = 0;
        _currentStroke = 0;
        _t0 = 0;
        _strokeOpen = false;
        if (_pad) lv_obj_invalidate(_pad);
    }

    static void _onClear(lv_event_t* e) {
        SignatureLockScreen* self = (SignatureLockScreen*)lv_event_get_user_data(e);
        self->_resetPad();
    }

    static void _onPadEvent(lv_event_t* e) {
        SignatureLockScreen* self = (SignatureLockScreen*)lv_event_get_user_data(e);
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_RELEASED) {
            if (self->_strokeOpen) {
                self->_currentStroke++;
                self->_strokeOpen = false;
            }
            return;
        }
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        lv_area_t a;
        lv_obj_get_coords(self->_pad, &a);
        int lx = p.x - a.x1;
        int ly = p.y - a.y1;
        if (lx < 0 || ly < 0 || lx >= PAD_W || ly >= PAD_H) return;
        if (self->_ptCount >= MAX_POINTS) return;
        uint32_t now = lv_tick_get();
        if (self->_ptCount == 0) self->_t0 = now;
        uint16_t t_rel = (uint16_t)((now - self->_t0) & 0xFFFF);
        self->_pts[self->_ptCount++] = {
            (int16_t)lx, (int16_t)ly, t_rel, self->_currentStroke
        };
        self->_strokeOpen = true;
        lv_obj_invalidate(self->_pad);
    }

    static void _onPadDraw(lv_event_t* e) {
        SignatureLockScreen* self = (SignatureLockScreen*)lv_event_get_user_data(e);
        if (self->_ptCount < 2) return;
        lv_obj_t* pad = self->_pad;
        lv_area_t a;
        lv_obj_get_coords(pad, &a);
        lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);

        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = gTheme->textDark;
        dsc.width = 2;
        dsc.round_start = 1;
        dsc.round_end = 1;

        for (int i = 1; i < self->_ptCount; ++i) {
            if (self->_pts[i].stroke != self->_pts[i - 1].stroke) continue;
            lv_point_t p1 = {
                (lv_coord_t)(a.x1 + self->_pts[i - 1].x),
                (lv_coord_t)(a.y1 + self->_pts[i - 1].y)
            };
            lv_point_t p2 = {
                (lv_coord_t)(a.x1 + self->_pts[i].x),
                (lv_coord_t)(a.y1 + self->_pts[i].y)
            };
            lv_draw_line(ctx, &dsc, &p1, &p2);
        }
    }

    static void _onUnlock(lv_event_t* e) {
        SignatureLockScreen* self = (SignatureLockScreen*)lv_event_get_user_data(e);
        if (self->_ptCount < 8) {
            lv_label_set_text(self->_hint, "Draw a longer signature");
            lv_obj_set_style_text_color(self->_hint, gTheme->errorText, LV_PART_MAIN);
            return;
        }
        std::vector<SignatureFeatures::Point> pts(
            self->_pts, self->_pts + self->_ptCount);
        float features[SignatureFeatures::FEATURE_DIM];
        float embedding[SignatureNet::EMB_DIM];
        SignatureFeatures::extract(pts, features);
        SignatureNet::forward(features, embedding);

        float centroid[SignatureNet::EMB_DIM];
        float threshold = 0.0f;
        if (!SignatureStore::load(centroid, &threshold)) {
            ScreenManager::getInstance().replaceCurrent(new HomeScreen());
            return;
        }
        float dist = SignatureNet::distance(embedding, centroid);
        if (dist <= threshold) {
            ScreenManager::getInstance().replaceCurrent(new HomeScreen());
            return;
        }
        self->_failCount++;
        self->_resetPad();
        if (self->_failCount >= MAX_FAILS) {
            ScreenManager::getInstance().replaceCurrent(new PinLockScreen());
            return;
        }
        char msg[48];
        snprintf(msg, sizeof(msg), "Signature mismatch (%d/%d)",
                 self->_failCount, MAX_FAILS);
        lv_label_set_text(self->_hint, msg);
        lv_obj_set_style_text_color(self->_hint, gTheme->errorText, LV_PART_MAIN);
    }
};
