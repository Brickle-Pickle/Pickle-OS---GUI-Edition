#pragma once
#include <lvgl.h>
#include <math.h>
#include "screen_base.h"
#include "../screen_manager.h"
#include "../theme/theme.h"
#include "../toast/toast_manager.h"
#include "editor3d/math3d.h"
#include "editor3d/mesh3d.h"
#include "editor3d/renderer3d.h"

// Minimalist 80s/90s style 3D editor. Supports translate/rotate/scale
// transforms on the model, freeform vertex+edge+face creation up to the
// MESH_MAX_* caps, and a per-model 5-color flat palette.
//
// The viewport is rendered via Renderer3D into an offscreen lv_canvas. A
// transparent overlay button on top of the canvas captures touch so we can
// distinguish drag (camera orbit) from tap (mode-specific action) and feed
// screen-space picks into the renderer.
class Editor3DScreen : public ScreenBase {
public:
    enum Mode {
        MODE_CAM = 0,
        MODE_MOVE,
        MODE_ROT,
        MODE_SCALE,
        MODE_VERT,
        MODE_EDGE,
        MODE_COLOR,
        MODE_COUNT
    };

    void onCreate() override {
        _screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(_screen, lv_color_hex(0x0A0420), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(_screen, lv_color_hex(0x1A0633), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(_screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_screen, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

        mesh_presets::buildCube(_mesh);

        _buildHeader();
        _buildViewport();
        _buildToolTabs();
        _buildActionBar();
        _updateModeUi();
        _requestRedraw();
    }

    void onDestroy() override {
        if (_redrawTimer) { lv_timer_del(_redrawTimer); _redrawTimer = nullptr; }
        _renderer.destroy();
        if (_screen) {
            lv_obj_del(_screen);
            _screen = nullptr;
        }
    }

private:
    // ------------- layout constants -------------
    // The viewport buffer dominates DRAM usage (240*160*2 = 76800 bytes).
    // Renderer3D prefers PSRAM and falls back to DRAM when external RAM is
    // not present, so the screen still works on the base ESP32-2432S028.
    static constexpr int HEADER_H = 26;
    static constexpr int VIEW_W   = 240;
    static constexpr int VIEW_H   = 160;
    static constexpr int VIEW_Y   = HEADER_H;
    static constexpr int TABS_Y   = HEADER_H + VIEW_H;
    static constexpr int TABS_H   = 28;
    static constexpr int ACT_Y    = TABS_Y + TABS_H;
    static constexpr int ACT_H    = 320 - ACT_Y;

    // ------------- editor state -------------
    Mesh                 _mesh;
    Renderer3D           _renderer{VIEW_W, VIEW_H};
    Mode                 _mode = MODE_CAM;

    bool                 _showGrid  = true;
    bool                 _showAxes  = true;
    bool                 _showFaces = true;

    int                  _selVert   = -1;
    int                  _selEdge   = -1;
    int                  _edgeFirst = -1;     // first vertex when building an edge
    uint8_t              _activeColor = 1;    // palette index for new edges/faces
    Vec3                 _cursor{0, 0, 0};
    bool                 _cursorVisible = false;

    // Camera orbit (radians, world units)
    float                _camYaw = 0.7f;
    float                _camPitch = 0.45f;
    float                _camDist = 3.5f;

    // Drag tracking
    bool                 _dragging = false;
    int                  _lastX = 0, _lastY = 0;
    int                  _dragStartX = 0, _dragStartY = 0;
    int                  _dragMaxMove = 0;

    // ------------- LVGL handles -------------
    lv_obj_t*            _viewportTouch = nullptr;
    lv_obj_t*            _statusLbl = nullptr;
    lv_obj_t*            _tabBtns[MODE_COUNT] = {nullptr};
    lv_obj_t*            _actionPane = nullptr;
    lv_timer_t*          _redrawTimer = nullptr;
    bool                 _redrawPending = false;

    // ------------- helpers -------------
    static lv_color_t _hdrBg()   { return lv_color_hex(0x1A0F38); }
    static lv_color_t _panelBg() { return lv_color_hex(0x140A2E); }
    static lv_color_t _btnBg()   { return lv_color_hex(0x2A1A55); }
    static lv_color_t _btnAcc()  { return lv_color_hex(0xFF2D95); }
    static lv_color_t _txt()     { return lv_color_hex(0xE0E0F8); }
    static lv_color_t _txtDim()  { return lv_color_hex(0x9080C0); }

    void _requestRedraw() {
        if (_redrawPending) return;
        _redrawPending = true;
        // 16ms ~= 60Hz cap. The timer fires once and re-arms only on next request.
        if (_redrawTimer) { lv_timer_del(_redrawTimer); _redrawTimer = nullptr; }
        _redrawTimer = lv_timer_create([](lv_timer_t* t) {
            auto* s = (Editor3DScreen*)t->user_data;
            s->_redrawPending = false;
            lv_timer_del(s->_redrawTimer);
            s->_redrawTimer = nullptr;
            s->_renderFrame();
        }, 16, this);
        lv_timer_set_repeat_count(_redrawTimer, 1);
    }

    void _renderFrame() {
        _renderer.setCamera(_camYaw, _camPitch, _camDist);
        _renderer.drawFrame(_mesh,
                            _showGrid, _showAxes, _showFaces,
                            _selVert, _selEdge,
                            (_mode == MODE_VERT) && _cursorVisible, _cursor,
                            lv_color_hex(0x05021A));
        _updateStatus();
    }

    // ------------- header -------------
    void _buildHeader() {
        lv_obj_t* h = lv_obj_create(_screen);
        lv_obj_set_size(h, 240, HEADER_H);
        lv_obj_set_pos(h, 0, 0);
        lv_obj_set_style_bg_color(h, _hdrBg(), LV_PART_MAIN);
        lv_obj_set_style_border_width(h, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(h, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(h, 0, LV_PART_MAIN);
        lv_obj_clear_flag(h, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* back = lv_btn_create(h);
        lv_obj_set_size(back, 30, 24);
        lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_set_style_bg_color(back, _btnBg(), LV_PART_MAIN);
        lv_obj_set_style_radius(back, 6, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(back, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(back, 0, LV_PART_MAIN);
        lv_obj_t* bi = lv_label_create(back);
        lv_label_set_text(bi, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_color(bi, _txt(), LV_PART_MAIN);
        lv_obj_center(bi);
        lv_obj_add_event_cb(back, [](lv_event_t*) {
            ScreenManager::getInstance().goBack();
        }, LV_EVENT_CLICKED, nullptr);

        lv_obj_t* title = lv_label_create(h);
        lv_label_set_text(title, "3D STUDIO");
        lv_obj_set_style_text_color(title, _btnAcc(), LV_PART_MAIN);
        lv_obj_set_style_text_font(title, gFontSmall, LV_PART_MAIN);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

        _statusLbl = lv_label_create(h);
        lv_label_set_text(_statusLbl, "");
        lv_obj_set_style_text_color(_statusLbl, _txtDim(), LV_PART_MAIN);
        lv_obj_set_style_text_font(_statusLbl, gFontSmall, LV_PART_MAIN);
        lv_obj_align(_statusLbl, LV_ALIGN_RIGHT_MID, -6, 0);
    }

    void _updateStatus() {
        if (!_statusLbl) return;
        lv_label_set_text_fmt(_statusLbl, "V%d E%d F%d",
                              _mesh.vertCount,
                              _mesh.edgeCount,
                              _mesh.faceCount);
    }

    // ------------- viewport (3D canvas) -------------
    void _buildViewport() {
        // Black frame backdrop so the canvas reads as a window even before
        // the first paint completes.
        lv_obj_t* frame = lv_obj_create(_screen);
        lv_obj_set_size(frame, VIEW_W, VIEW_H);
        lv_obj_set_pos(frame, 0, VIEW_Y);
        lv_obj_set_style_bg_color(frame, lv_color_hex(0x05021A), LV_PART_MAIN);
        lv_obj_set_style_border_width(frame, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(frame, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(frame, 0, LV_PART_MAIN);
        lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

        if (!_renderer.create(_screen, 0, VIEW_Y)) {
            ToastManager::getInstance().showToast("3D buffer alloc failed",
                                                  ToastType::ALERT);
            return;
        }

        // Transparent capture surface on top of the canvas, the same size.
        _viewportTouch = lv_obj_create(_screen);
        lv_obj_set_size(_viewportTouch, VIEW_W, VIEW_H);
        lv_obj_set_pos(_viewportTouch, 0, VIEW_Y);
        lv_obj_set_style_bg_opa(_viewportTouch, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(_viewportTouch, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_viewportTouch, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_viewportTouch, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_viewportTouch, LV_OBJ_FLAG_SCROLLABLE);
        // Touch capture: clickable so we receive PRESSED/PRESSING/RELEASED.
        lv_obj_add_flag(_viewportTouch, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(_viewportTouch, this);

        lv_obj_add_event_cb(_viewportTouch, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_obj_get_user_data(lv_event_get_target(e));
            s->_onViewportEvent(e);
        }, LV_EVENT_ALL, nullptr);
    }

    void _onViewportEvent(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t* indev = lv_indev_get_act();
        if (!indev) return;
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        int lx = p.x;
        int ly = p.y - VIEW_Y;

        switch (code) {
            case LV_EVENT_PRESSED:
                _dragging = true;
                _lastX = p.x;
                _lastY = p.y;
                _dragStartX = p.x;
                _dragStartY = p.y;
                _dragMaxMove = 0;
                break;
            case LV_EVENT_PRESSING: {
                if (!_dragging) break;
                int dx = p.x - _lastX;
                int dy = p.y - _lastY;
                int totalDx = p.x - _dragStartX;
                int totalDy = p.y - _dragStartY;
                int m = abs(totalDx) > abs(totalDy) ? abs(totalDx) : abs(totalDy);
                if (m > _dragMaxMove) _dragMaxMove = m;
                if (_dragMaxMove > 4) {
                    // Camera orbit: drag X -> yaw, drag Y -> pitch
                    _camYaw += dx * 0.012f;
                    _camPitch += dy * 0.012f;
                    if (_camPitch > 1.45f) _camPitch = 1.45f;
                    if (_camPitch < -1.45f) _camPitch = -1.45f;
                    _requestRedraw();
                }
                _lastX = p.x;
                _lastY = p.y;
                break;
            }
            case LV_EVENT_RELEASED: {
                bool wasTap = _dragging && _dragMaxMove <= 4;
                _dragging = false;
                if (wasTap) _onTap(lx, ly);
                break;
            }
            case LV_EVENT_PRESS_LOST:
                _dragging = false;
                break;
            default: break;
        }
    }

    // ------------- tap dispatch -------------
    void _onTap(int x, int y) {
        // Sync the renderer's camera with the editor's state so picks use
        // the same projection as the last on-screen frame.
        _renderer.setCamera(_camYaw, _camPitch, _camDist);
        switch (_mode) {
            case MODE_CAM:
            case MODE_MOVE:
            case MODE_ROT:
            case MODE_SCALE:
                // Try to select a vertex; clear selection on empty tap.
                _selVert = _renderer.pickVertex(_mesh, x, y, 12);
                _selEdge = (_selVert < 0) ? _renderer.pickEdge(_mesh, x, y, 10) : -1;
                _requestRedraw();
                break;
            case MODE_VERT: {
                // Try first to pick an existing vertex (for selection/delete).
                int pv = _renderer.pickVertex(_mesh, x, y, 12);
                if (pv >= 0) {
                    _selVert = pv;
                } else {
                    // Otherwise raycast to the y=0 plane and place a vertex.
                    Vec3 hit;
                    if (_renderer.screenToGround(x, y, hit)) {
                        // The mesh has its own transform — store the vertex
                        // in the mesh local space so the placement matches
                        // the visible cursor.
                        Vec3 local = _worldToLocal(hit);
                        int idx = _mesh.addVertex(local);
                        if (idx < 0) {
                            ToastManager::getInstance().showToast(
                                "Vertex limit reached", ToastType::ALERT);
                        } else {
                            _selVert = idx;
                        }
                    }
                }
                _requestRedraw();
                break;
            }
            case MODE_EDGE: {
                int pv = _renderer.pickVertex(_mesh, x, y, 12);
                if (pv >= 0) {
                    if (_edgeFirst < 0) {
                        _edgeFirst = pv;
                        _selVert = pv;
                    } else if (pv == _edgeFirst) {
                        _edgeFirst = -1;
                        _selVert = -1;
                    } else {
                        int idx = _mesh.addEdge(_edgeFirst, pv, _activeColor);
                        if (idx < 0) {
                            ToastManager::getInstance().showToast(
                                "Cannot add edge", ToastType::ALERT);
                        } else {
                            _selEdge = idx;
                        }
                        _edgeFirst = -1;
                        _selVert = -1;
                    }
                } else {
                    int pe = _renderer.pickEdge(_mesh, x, y, 10);
                    if (pe >= 0) _selEdge = pe;
                }
                _requestRedraw();
                _updateActionLabels();
                break;
            }
            case MODE_COLOR: {
                int pe = _renderer.pickEdge(_mesh, x, y, 10);
                if (pe >= 0) {
                    _mesh.edges[pe].color = _activeColor;
                    _selEdge = pe;
                    _requestRedraw();
                }
                break;
            }
            default: break;
        }
    }

    // Inverse of mesh.modelMatrix(). Used to convert a world-space hit point
    // back into the mesh's local coordinates before storing a vertex.
    Vec3 _worldToLocal(const Vec3& w) const {
        Mat4 inv = Mat4::scale(1.0f / _mesh.scale.x,
                               1.0f / _mesh.scale.y,
                               1.0f / _mesh.scale.z)
                 * Mat4::rotateZ(-_mesh.rotation.z)
                 * Mat4::rotateX(-_mesh.rotation.x)
                 * Mat4::rotateY(-_mesh.rotation.y)
                 * Mat4::translate(-_mesh.position.x,
                                   -_mesh.position.y,
                                   -_mesh.position.z);
        return inv.transform(w);
    }

    // ------------- mode tabs -------------
    void _buildToolTabs() {
        lv_obj_t* bar = lv_obj_create(_screen);
        lv_obj_set_size(bar, 240, TABS_H);
        lv_obj_set_pos(bar, 0, TABS_Y);
        lv_obj_set_style_bg_color(bar, _hdrBg(), LV_PART_MAIN);
        lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

        const char* labels[MODE_COUNT] = {
            "CAM", "MOV", "ROT", "SCL", "VRT", "EDG", "COL"
        };
        const int tabW = 240 / MODE_COUNT; // 34 px
        for (int i = 0; i < MODE_COUNT; ++i) {
            lv_obj_t* b = lv_btn_create(bar);
            lv_obj_set_size(b, tabW - 2, TABS_H - 4);
            lv_obj_set_pos(b, i * tabW + 1, 2);
            lv_obj_set_style_bg_color(b, _btnBg(), LV_PART_MAIN);
            lv_obj_set_style_bg_color(b, _btnAcc(), LV_STATE_CHECKED);
            lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(b, LV_OBJ_FLAG_CHECKABLE);
            _tabBtns[i] = b;

            lv_obj_t* lbl = lv_label_create(b);
            lv_label_set_text(lbl, labels[i]);
            lv_obj_set_style_text_color(lbl, _txt(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
            lv_obj_center(lbl);

            // Stash the mode index on the button for the callback.
            lv_obj_set_user_data(b, (void*)(intptr_t)i);
            // Group context: the screen pointer rides on the event user data.
            lv_obj_add_event_cb(b, [](lv_event_t* e) {
                auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
                int m = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
                s->_setMode((Mode)m);
            }, LV_EVENT_CLICKED, this);
        }
    }

    void _setMode(Mode m) {
        _mode = m;
        _edgeFirst = -1;
        _cursorVisible = (m == MODE_VERT);
        _updateModeUi();
        _requestRedraw();
    }

    void _updateModeUi() {
        // Sync the checked state of the tab strip with the current mode.
        for (int i = 0; i < MODE_COUNT; ++i) {
            if (!_tabBtns[i]) continue;
            if (i == (int)_mode) lv_obj_add_state(_tabBtns[i], LV_STATE_CHECKED);
            else                  lv_obj_clear_state(_tabBtns[i], LV_STATE_CHECKED);
        }
        _buildActionBar();
    }

    // ------------- action bar -------------
    void _buildActionBar() {
        if (_actionPane) { lv_obj_del(_actionPane); _actionPane = nullptr; }
        // Any per-pane child handles need to be invalidated so we never
        // dereference a deleted LVGL object after rebuilding.
        _edgeStatusLbl = nullptr;
        _actionPane = lv_obj_create(_screen);
        lv_obj_set_size(_actionPane, 240, ACT_H);
        lv_obj_set_pos(_actionPane, 0, ACT_Y);
        lv_obj_set_style_bg_color(_actionPane, _panelBg(), LV_PART_MAIN);
        lv_obj_set_style_border_width(_actionPane, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(_actionPane, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(_actionPane, 0, LV_PART_MAIN);
        lv_obj_clear_flag(_actionPane, LV_OBJ_FLAG_SCROLLABLE);

        switch (_mode) {
            case MODE_CAM:   _buildCamPane();    break;
            case MODE_MOVE:  _buildAxisPane(0);  break;
            case MODE_ROT:   _buildAxisPane(1);  break;
            case MODE_SCALE: _buildAxisPane(2);  break;
            case MODE_VERT:  _buildVertPane();   break;
            case MODE_EDGE:  _buildEdgePane();   break;
            case MODE_COLOR: _buildColorPane();  break;
            default: break;
        }
    }

    // Small helper to create a flat button with a centered label. The
    // background color defaults to the standard button background; pass a
    // tinted color (e.g. red for destructive actions) to override.
    lv_obj_t* _mkBtnC(lv_obj_t* parent, int x, int y, int w, int h,
                      const char* text, lv_color_t bg) {
        lv_obj_t* b = lv_btn_create(parent);
        lv_obj_set_size(b, w, h);
        lv_obj_set_pos(b, x, y);
        lv_obj_set_style_bg_color(b, bg, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, _btnAcc(), LV_STATE_PRESSED);
        lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
        lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(b);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, _txt(), LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
        lv_obj_center(lbl);
        return b;
    }
    lv_obj_t* _mkBtn(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* text) {
        return _mkBtnC(parent, x, y, w, h, text, _btnBg());
    }

    void _buildCamPane() {
        // Row 1: Reset cam / Grid / Axes / Fill toggles
        const char* lbls[4] = { "RESET", "GRID", "AXES", "FILL" };
        auto cb = [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            int id = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            switch (id) {
                case 0:
                    s->_camYaw = 0.7f;
                    s->_camPitch = 0.45f;
                    s->_camDist = 3.5f;
                    break;
                case 1: s->_showGrid  = !s->_showGrid;  break;
                case 2: s->_showAxes  = !s->_showAxes;  break;
                case 3: s->_showFaces = !s->_showFaces; break;
            }
            s->_requestRedraw();
        };
        for (int i = 0; i < 4; ++i) {
            lv_obj_t* b = _mkBtn(_actionPane, 4 + i * 59, 4, 56, 24, lbls[i]);
            lv_obj_set_user_data(b, (void*)(intptr_t)i);
            lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);
        }
        // Row 2: Preset selectors + Clear
        const char* p[5] = { "CUBE", "PYR", "DIAM", "PLN", "CLR" };
        auto pcb = [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            int id = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            s->_selVert = -1;
            s->_selEdge = -1;
            s->_edgeFirst = -1;
            switch (id) {
                case 0: mesh_presets::buildCube(s->_mesh);    break;
                case 1: mesh_presets::buildPyramid(s->_mesh); break;
                case 2: mesh_presets::buildDiamond(s->_mesh); break;
                case 3: mesh_presets::buildPlane(s->_mesh);   break;
                case 4: s->_mesh.clear();                     break;
            }
            s->_requestRedraw();
        };
        for (int i = 0; i < 5; ++i) {
            lv_obj_t* b = _mkBtnC(_actionPane, 2 + i * 47, 30, 45, 24, p[i],
                                  i == 4 ? lv_color_hex(0x551122) : _btnBg());
            lv_obj_set_user_data(b, (void*)(intptr_t)i);
            lv_obj_add_event_cb(b, pcb, LV_EVENT_CLICKED, this);
        }
    }

    // axisKind: 0=translate, 1=rotate, 2=scale.
    void _buildAxisPane(int axisKind) {
        // 3 rows × 2 cols of -/+ buttons per axis. Tight fit in 62 px tall:
        // we lay out 3 columns (X/Y/Z) each with a label header and -/+
        // buttons stacked horizontally underneath.
        const char* axisNames[3] = { "X", "Y", "Z" };
        const lv_color_t axisCols[3] = {
            lv_color_hex(0xFF3060),
            lv_color_hex(0x60FF60),
            lv_color_hex(0x4080FF)
        };

        struct Ctx { Editor3DScreen* s; int axisKind; int axis; int dir; };
        for (int a = 0; a < 3; ++a) {
            int col = 4 + a * 78;

            lv_obj_t* lbl = lv_label_create(_actionPane);
            lv_label_set_text(lbl, axisNames[a]);
            lv_obj_set_style_text_color(lbl, axisCols[a], LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
            lv_obj_set_pos(lbl, col + 32, 2);

            lv_obj_t* bm = _mkBtn(_actionPane, col, 14, 36, 22, "-");
            lv_obj_t* bp = _mkBtn(_actionPane, col + 38, 14, 36, 22, "+");
            // Persist the axis/dir/kind context on each button.
            auto* cM = new Ctx{ this, axisKind, a, -1 };
            auto* cP = new Ctx{ this, axisKind, a, +1 };
            lv_obj_set_user_data(bm, cM);
            lv_obj_set_user_data(bp, cP);
            auto cb = [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                c->s->_applyTransform(c->axisKind, c->axis, c->dir);
            };
            auto cbDel = [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                delete c;
            };
            lv_obj_add_event_cb(bm, cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(bp, cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(bm, cbDel, LV_EVENT_DELETE, nullptr);
            lv_obj_add_event_cb(bp, cbDel, LV_EVENT_DELETE, nullptr);
        }
        // Hint label spanning the bottom row.
        lv_obj_t* hint = lv_label_create(_actionPane);
        const char* h = (axisKind == 0) ? "TRANSLATE 0.25u"
                       : (axisKind == 1) ? "ROTATE 15deg"
                       : "SCALE x1.1";
        lv_label_set_text(hint, h);
        lv_obj_set_style_text_color(hint, _txtDim(), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, gFontSmall, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -2);
    }

    void _applyTransform(int kind, int axis, int dir) {
        const float dPos = 0.25f * dir;
        // 15 degrees in radians (pi/12), constant inlined to avoid pulling
        // M_PI which is not always defined on Arduino-ESP32 builds.
        const float dRot = 0.2617994f * dir;
        const float dScale = (dir > 0) ? 1.1f : (1.0f / 1.1f);
        // Selected vertex moves alone when in MOVE mode. Otherwise the whole
        // model transforms.
        if (kind == 0 && _selVert >= 0 && _mode == MODE_MOVE) {
            Vec3& v = _mesh.verts[_selVert];
            if (axis == 0) v.x += dPos;
            if (axis == 1) v.y += dPos;
            if (axis == 2) v.z += dPos;
        } else if (kind == 0) {
            if (axis == 0) _mesh.position.x += dPos;
            if (axis == 1) _mesh.position.y += dPos;
            if (axis == 2) _mesh.position.z += dPos;
        } else if (kind == 1) {
            if (axis == 0) _mesh.rotation.x += dRot;
            if (axis == 1) _mesh.rotation.y += dRot;
            if (axis == 2) _mesh.rotation.z += dRot;
        } else if (kind == 2) {
            if (axis == 0) _mesh.scale.x *= dScale;
            if (axis == 1) _mesh.scale.y *= dScale;
            if (axis == 2) _mesh.scale.z *= dScale;
            // Clamp scale so the model never collapses to zero.
            if (_mesh.scale.x < 0.05f) _mesh.scale.x = 0.05f;
            if (_mesh.scale.y < 0.05f) _mesh.scale.y = 0.05f;
            if (_mesh.scale.z < 0.05f) _mesh.scale.z = 0.05f;
        }
        _requestRedraw();
    }

    void _buildVertPane() {
        // Cursor nudge buttons (3 columns × 2 rows of - / +)
        struct Ctx { Editor3DScreen* s; int axis; int dir; };
        const char* names[3] = { "Xc", "Yc", "Zc" };
        for (int a = 0; a < 3; ++a) {
            int col = 4 + a * 50;
            lv_obj_t* lbl = lv_label_create(_actionPane);
            lv_label_set_text(lbl, names[a]);
            lv_obj_set_style_text_color(lbl, _txtDim(), LV_PART_MAIN);
            lv_obj_set_style_text_font(lbl, gFontSmall, LV_PART_MAIN);
            lv_obj_set_pos(lbl, col + 18, 2);

            lv_obj_t* bm = _mkBtn(_actionPane, col, 14, 22, 22, "-");
            lv_obj_t* bp = _mkBtn(_actionPane, col + 24, 14, 22, 22, "+");
            auto* cM = new Ctx{ this, a, -1 };
            auto* cP = new Ctx{ this, a, +1 };
            lv_obj_set_user_data(bm, cM);
            lv_obj_set_user_data(bp, cP);
            auto cb = [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                float step = 0.25f * c->dir;
                if (c->axis == 0) c->s->_cursor.x += step;
                if (c->axis == 1) c->s->_cursor.y += step;
                if (c->axis == 2) c->s->_cursor.z += step;
                c->s->_cursorVisible = true;
                c->s->_requestRedraw();
            };
            auto cbDel = [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                delete c;
            };
            lv_obj_add_event_cb(bm, cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(bp, cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(bm, cbDel, LV_EVENT_DELETE, nullptr);
            lv_obj_add_event_cb(bp, cbDel, LV_EVENT_DELETE, nullptr);
        }
        // ADD + DEL on the right.
        lv_obj_t* add = _mkBtnC(_actionPane, 158, 4, 78, 22, "ADD HERE",
                                lv_color_hex(0x224477));
        lv_obj_add_event_cb(add, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            int idx = s->_mesh.addVertex(s->_worldToLocal(s->_cursor));
            if (idx < 0) {
                ToastManager::getInstance().showToast("Vertex limit reached",
                                                      ToastType::ALERT);
            } else {
                s->_selVert = idx;
            }
            s->_requestRedraw();
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* del = _mkBtnC(_actionPane, 158, 30, 78, 22, "DEL SEL",
                                lv_color_hex(0x551122));
        lv_obj_add_event_cb(del, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            if (s->_selVert >= 0) {
                s->_mesh.removeVertex(s->_selVert);
                s->_selVert = -1;
                s->_requestRedraw();
            }
        }, LV_EVENT_CLICKED, this);

        // Reset cursor button at the bottom-left.
        lv_obj_t* rc = _mkBtnC(_actionPane, 4, 38, 60, 20, "CRSR 0",
                               lv_color_hex(0x2A3055));
        lv_obj_add_event_cb(rc, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            s->_cursor = Vec3(0, 0, 0);
            s->_requestRedraw();
        }, LV_EVENT_CLICKED, this);
    }

    void _buildEdgePane() {
        // Status label (top row)
        lv_obj_t* st = lv_label_create(_actionPane);
        lv_label_set_text(st, "");
        lv_obj_set_style_text_color(st, _txtDim(), LV_PART_MAIN);
        lv_obj_set_style_text_font(st, gFontSmall, LV_PART_MAIN);
        lv_obj_set_pos(st, 6, 4);
        _edgeStatusLbl = st;
        _updateActionLabels();

        // Active color preview swatch.
        lv_obj_t* swatch = lv_obj_create(_actionPane);
        lv_obj_set_size(swatch, 18, 14);
        lv_obj_set_pos(swatch, 218, 4);
        lv_obj_set_style_bg_color(swatch,
            _mesh.palette[_activeColor % MESH_PALETTE_SIZE], LV_PART_MAIN);
        lv_obj_set_style_border_width(swatch, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(swatch, _txt(), LV_PART_MAIN);
        lv_obj_set_style_radius(swatch, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_all(swatch, 0, LV_PART_MAIN);
        lv_obj_clear_flag(swatch, LV_OBJ_FLAG_SCROLLABLE);

        // Cancel and delete on the bottom row.
        lv_obj_t* cancel = _mkBtn(_actionPane, 4, 28, 70, 26, "CANCEL");
        lv_obj_add_event_cb(cancel, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            s->_edgeFirst = -1;
            s->_selVert = -1;
            s->_selEdge = -1;
            s->_updateActionLabels();
            s->_requestRedraw();
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* del = _mkBtnC(_actionPane, 78, 28, 80, 26, "DEL EDGE",
                                lv_color_hex(0x551122));
        lv_obj_add_event_cb(del, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            if (s->_selEdge >= 0) {
                s->_mesh.removeEdge(s->_selEdge);
                s->_selEdge = -1;
                s->_requestRedraw();
            }
        }, LV_EVENT_CLICKED, this);

        // Face quick-build button: turns the last 3 selected vertices into a
        // triangle. Keeps the editor reachable without a full face mode.
        lv_obj_t* face = _mkBtnC(_actionPane, 162, 28, 74, 26, "+FACE",
                                 lv_color_hex(0x224477));
        lv_obj_add_event_cb(face, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            s->_addFaceFromEdges();
        }, LV_EVENT_CLICKED, this);
    }

    // Try to turn the most recently selected edge plus the selected vertex
    // (or the last two edges) into a face. The user picks a vertex first
    // then taps +FACE, which creates a triangle from the last edge plus
    // that vertex.
    void _addFaceFromEdges() {
        if (_selEdge < 0 || _selVert < 0) {
            ToastManager::getInstance().showToast(
                "Pick edge + vertex first", ToastType::ALERT);
            return;
        }
        const Edge& e = _mesh.edges[_selEdge];
        int a = e.a, b = e.b, c = _selVert;
        if (c == a || c == b) {
            ToastManager::getInstance().showToast(
                "Vertex already on edge", ToastType::ALERT);
            return;
        }
        int idx = _mesh.addFace(a, b, c, _activeColor);
        if (idx < 0) {
            ToastManager::getInstance().showToast(
                "Cannot add face", ToastType::ALERT);
            return;
        }
        _requestRedraw();
    }

    void _buildColorPane() {
        struct Ctx { Editor3DScreen* s; int idx; };
        // 5 palette swatches in row 1.
        for (int i = 0; i < MESH_PALETTE_SIZE; ++i) {
            lv_obj_t* b = lv_btn_create(_actionPane);
            lv_obj_set_size(b, 36, 26);
            lv_obj_set_pos(b, 6 + i * 46, 4);
            lv_obj_set_style_bg_color(b, _mesh.palette[i], LV_PART_MAIN);
            lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
            lv_obj_set_style_border_color(b,
                (i == _activeColor) ? _txt() : lv_color_hex(0x000000),
                LV_PART_MAIN);
            lv_obj_set_style_border_width(b, (i == _activeColor) ? 2 : 0,
                                           LV_PART_MAIN);
            lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
            lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            auto* c = new Ctx{ this, i };
            lv_obj_set_user_data(b, c);
            lv_obj_add_event_cb(b, [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                c->s->_activeColor = (uint8_t)c->idx;
                if (c->s->_selEdge >= 0) {
                    c->s->_mesh.edges[c->s->_selEdge].color = (uint8_t)c->idx;
                }
                c->s->_buildColorPane();
                c->s->_requestRedraw();
            }, LV_EVENT_CLICKED, nullptr);
            lv_obj_add_event_cb(b, [](lv_event_t* e) {
                Ctx* c = (Ctx*)lv_obj_get_user_data(lv_event_get_target(e));
                delete c;
            }, LV_EVENT_DELETE, nullptr);
        }
        // Cycle the active color through a fixed neon palette so the user
        // can swap any of the 5 slots without a full RGB picker.
        lv_obj_t* cyc = _mkBtnC(_actionPane, 4, 34, 110, 24, "CYCLE SLOT",
                                lv_color_hex(0x2A3055));
        lv_obj_add_event_cb(cyc, [](lv_event_t* e) {
            auto* s = (Editor3DScreen*)lv_event_get_user_data(e);
            s->_cycleActiveSlot();
        }, LV_EVENT_CLICKED, this);

        lv_obj_t* hint = lv_label_create(_actionPane);
        lv_label_set_text(hint, "Tap edge to paint");
        lv_obj_set_style_text_color(hint, _txtDim(), LV_PART_MAIN);
        lv_obj_set_style_text_font(hint, gFontSmall, LV_PART_MAIN);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -6, -4);
    }

    void _cycleActiveSlot() {
        // Walk the active palette index through a 12-color neon wheel.
        static const lv_color_t wheel[] = {
            lv_color_hex(0xFF2D95),
            lv_color_hex(0x00E5FF),
            lv_color_hex(0xFFD400),
            lv_color_hex(0x39FF14),
            lv_color_hex(0xFF8C1A),
            lv_color_hex(0xB000FF),
            lv_color_hex(0xFF4040),
            lv_color_hex(0x40FFD0),
            lv_color_hex(0xFFFFFF),
            lv_color_hex(0xA0A0A0),
            lv_color_hex(0x6060FF),
            lv_color_hex(0xFF60A0),
        };
        const int n = sizeof(wheel) / sizeof(wheel[0]);
        int slot = _activeColor % MESH_PALETTE_SIZE;
        // Find the next color in the wheel that doesn't match the current.
        lv_color_t cur = _mesh.palette[slot];
        int cur_idx = 0;
        for (int i = 0; i < n; ++i) {
            if (lv_color_to32(wheel[i]) == lv_color_to32(cur)) {
                cur_idx = i;
                break;
            }
        }
        _mesh.palette[slot] = wheel[(cur_idx + 1) % n];
        _buildColorPane();
        _requestRedraw();
    }

    // ------------- status label inside the EDGE pane -------------
    lv_obj_t* _edgeStatusLbl = nullptr;
    void _updateActionLabels() {
        if (_mode != MODE_EDGE || !_edgeStatusLbl) return;
        if (_edgeFirst < 0) {
            lv_label_set_text(_edgeStatusLbl, "Pick vertex A");
        } else {
            lv_label_set_text_fmt(_edgeStatusLbl, "A=%d  pick B", _edgeFirst);
        }
    }
};
