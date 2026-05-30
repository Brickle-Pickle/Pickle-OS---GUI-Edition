#pragma once
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <stdlib.h>
#include "math3d.h"
#include "mesh3d.h"

// Software 3D rasterizer that targets an lv_canvas buffer.
//
// Coordinate convention:
//   - Right-handed world. +Y is up, +Z is forward (toward viewer).
//   - Camera looks down the world origin from a yaw/pitch orbit.
//   - Pixels are stored as packed lv_color_t (RGB565 with LV_COLOR_DEPTH=16).
//
// The render pipeline composes a full frame into the buffer on every call to
// drawFrame(); call lv_obj_invalidate() on the canvas afterwards so LVGL
// pushes it to the display.
class Renderer3D {
public:
    Renderer3D(int w, int h) : _w(w), _h(h) {}

    bool create(lv_obj_t* parent, int x, int y) {
        size_t bytes = (size_t)_w * (size_t)_h * sizeof(lv_color_t);
        // PSRAM first; fall back to DRAM if the board has no external RAM.
        _buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!_buf) _buf = (lv_color_t*)heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
        if (!_buf) return false;
        _canvas = lv_canvas_create(parent);
        lv_canvas_set_buffer(_canvas, _buf, _w, _h, LV_IMG_CF_TRUE_COLOR);
        lv_obj_set_pos(_canvas, x, y);
        return true;
    }

    void destroy() {
        if (_canvas) { lv_obj_del(_canvas); _canvas = nullptr; }
        if (_buf) { heap_caps_free(_buf); _buf = nullptr; }
    }

    lv_obj_t* canvas() const { return _canvas; }
    int width() const { return _w; }
    int height() const { return _h; }

    void setCamera(float yaw, float pitch, float dist) {
        _camYaw = yaw;
        _camPitch = pitch;
        _camDist = dist;
    }

    // Render the full frame in one pass. The caller passes everything it
    // wants drawn so the renderer can do a single screen-space transform per
    // vertex (no LVGL allocations, no per-edge draw stack).
    void drawFrame(const Mesh& mesh,
                   bool showGrid,
                   bool showAxes,
                   bool showFaces,
                   int selectedVert,
                   int selectedEdge,
                   bool hasCursor,
                   const Vec3& cursorPos,
                   lv_color_t bg) {
        if (!_buf) return;
        _clear(bg);
        _buildViewProj();

        if (showGrid) _drawGrid();
        if (showAxes) _drawAxes();

        Mat4 model = mesh.modelMatrix();
        Mat4 mvp = _view * model;

        // Project each vertex once. Mark vertices behind the near plane as
        // invalid so edges/faces that depend on them can be skipped.
        Vec3 cam[MESH_MAX_VERTS];
        int sx[MESH_MAX_VERTS];
        int sy[MESH_MAX_VERTS];
        bool ok[MESH_MAX_VERTS];
        for (uint8_t i = 0; i < mesh.vertCount; ++i) {
            cam[i] = mvp.transform(mesh.verts[i]);
            ok[i] = _project(cam[i], sx[i], sy[i]);
        }

        // Filled faces first (so wire stays on top). Painter algorithm by
        // average depth keeps simple overlap right enough for a tiny mesh
        // without a real z-buffer.
        if (showFaces && mesh.faceCount > 0) {
            uint8_t order[MESH_MAX_FACES];
            float depth[MESH_MAX_FACES];
            uint8_t n = 0;
            for (uint8_t i = 0; i < mesh.faceCount; ++i) {
                const Face& f = mesh.faces[i];
                if (!ok[f.a] || !ok[f.b] || !ok[f.c]) continue;
                order[n] = i;
                // averaged camera-space Z; smaller (more negative) is farther
                depth[n] = (cam[f.a].z + cam[f.b].z + cam[f.c].z) * (1.0f / 3.0f);
                ++n;
            }
            // Insertion sort: ascending depth (farthest first).
            for (uint8_t i = 1; i < n; ++i) {
                uint8_t k = order[i];
                float d = depth[i];
                int j = i;
                while (j > 0 && depth[j - 1] > d) {
                    order[j] = order[j - 1];
                    depth[j] = depth[j - 1];
                    --j;
                }
                order[j] = k;
                depth[j] = d;
            }
            for (uint8_t i = 0; i < n; ++i) {
                const Face& f = mesh.faces[order[i]];
                lv_color_t c = mesh.palette[f.color % MESH_PALETTE_SIZE];
                // Backface cull in screen space.
                int ax = sx[f.a], ay = sy[f.a];
                int bx = sx[f.b], by = sy[f.b];
                int cx = sx[f.c], cy = sy[f.c];
                int signedArea = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
                if (signedArea <= 0) continue;
                _fillTriangle(ax, ay, bx, by, cx, cy, c);
            }
        }

        // Wireframe edges. Selected edge gets the highlight color.
        for (uint8_t i = 0; i < mesh.edgeCount; ++i) {
            const Edge& e = mesh.edges[i];
            if (!ok[e.a] || !ok[e.b]) continue;
            lv_color_t c = (i == selectedEdge)
                               ? lv_color_hex(0xFFFFFF)
                               : mesh.palette[e.color % MESH_PALETTE_SIZE];
            _drawLine(sx[e.a], sy[e.a], sx[e.b], sy[e.b], c);
        }

        // Vertex handles. Drawn last so they always sit on top of edges.
        for (uint8_t i = 0; i < mesh.vertCount; ++i) {
            if (!ok[i]) continue;
            lv_color_t c = (i == selectedVert)
                               ? lv_color_hex(0xFFFFFF)
                               : lv_color_hex(0xB0B0B0);
            int r = (i == selectedVert) ? 3 : 2;
            _fillRect(sx[i] - r, sy[i] - r, r * 2 + 1, r * 2 + 1, c);
        }

        if (hasCursor) {
            Vec3 cc = _view.transform(cursorPos);
            int cx, cy;
            if (_project(cc, cx, cy)) {
                lv_color_t hi = lv_color_hex(0xFFEA00);
                _drawLine(cx - 5, cy, cx + 5, cy, hi);
                _drawLine(cx, cy - 5, cx, cy + 5, hi);
                _fillRect(cx - 1, cy - 1, 3, 3, hi);
            }
        }

        if (_canvas) lv_obj_invalidate(_canvas);
    }

    // Find the on-screen vertex (post-MVP) within `pickRadius` of (px, py).
    // Returns -1 if nothing close. Useful for tap-to-select.
    int pickVertex(const Mesh& mesh, int px, int py, int pickRadius = 12) {
        _buildViewProj();
        Mat4 mvp = _view * mesh.modelMatrix();
        int best = -1;
        int bestDist = pickRadius * pickRadius;
        for (uint8_t i = 0; i < mesh.vertCount; ++i) {
            Vec3 c = mvp.transform(mesh.verts[i]);
            int sx, sy;
            if (!_project(c, sx, sy)) continue;
            int dx = sx - px;
            int dy = sy - py;
            int d2 = dx * dx + dy * dy;
            if (d2 < bestDist) { bestDist = d2; best = i; }
        }
        return best;
    }

    // Pick the edge whose midpoint sits closest to (px,py) within range.
    int pickEdge(const Mesh& mesh, int px, int py, int pickRadius = 10) {
        _buildViewProj();
        Mat4 mvp = _view * mesh.modelMatrix();
        int best = -1;
        int bestDist = pickRadius * pickRadius;
        for (uint8_t i = 0; i < mesh.edgeCount; ++i) {
            const Edge& e = mesh.edges[i];
            Vec3 a = mvp.transform(mesh.verts[e.a]);
            Vec3 b = mvp.transform(mesh.verts[e.b]);
            int ax, ay, bx, by;
            if (!_project(a, ax, ay) || !_project(b, bx, by)) continue;
            int mx = (ax + bx) / 2;
            int my = (ay + by) / 2;
            int dx = mx - px;
            int dy = my - py;
            int d2 = dx * dx + dy * dy;
            if (d2 < bestDist) { bestDist = d2; best = i; }
        }
        return best;
    }

    // Unproject a tap onto the Y=0 ground plane in world coordinates.
    // Returns false when the ray is parallel to the plane.
    bool screenToGround(int px, int py, Vec3& out) {
        _buildViewProj();
        // Build ray in camera space (camera looks down -Z, +Y up).
        float fx = _focal();
        float dx = ((float)px - _w * 0.5f) / fx;
        float dy = -((float)py - _h * 0.5f) / fx;
        Vec3 dirCam(dx, dy, -1.0f);
        // Camera -> world: inverse of view rotation. The view is yaw->pitch
        // then a translate by -dist along Z, so the inverse rotation is
        // rotateY(yaw) * rotateX(pitch).
        Mat4 invRot = Mat4::rotateY(_camYaw) * Mat4::rotateX(_camPitch);
        Vec3 dirWorld = invRot.transformDir(dirCam);
        Vec3 camPos = invRot.transform(Vec3(0, 0, _camDist));
        if (fabsf(dirWorld.y) < 1e-4f) return false;
        float t = -camPos.y / dirWorld.y;
        if (t <= 0.0f) return false;
        out = camPos + dirWorld * t;
        return true;
    }

private:
    // -------- low-level raster --------
    inline void _putPx(int x, int y, lv_color_t c) {
        if ((unsigned)x >= (unsigned)_w || (unsigned)y >= (unsigned)_h) return;
        _buf[y * _w + x] = c;
    }

    void _clear(lv_color_t c) {
        // 16-bit fill loop; the buffer is contiguous lv_color_t entries.
        lv_color_t* p = _buf;
        int n = _w * _h;
        for (int i = 0; i < n; ++i) p[i] = c;
    }

    void _fillRect(int x, int y, int w, int h, lv_color_t c) {
        int x0 = x < 0 ? 0 : x;
        int y0 = y < 0 ? 0 : y;
        int x1 = (x + w > _w) ? _w : (x + w);
        int y1 = (y + h > _h) ? _h : (y + h);
        for (int yy = y0; yy < y1; ++yy) {
            lv_color_t* row = _buf + yy * _w;
            for (int xx = x0; xx < x1; ++xx) row[xx] = c;
        }
    }

    // Bresenham, clipped per pixel (cheap enough for ~40 edges).
    void _drawLine(int x0, int y0, int x1, int y1, lv_color_t c) {
        int dx = abs(x1 - x0);
        int dy = -abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            _putPx(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // Filled triangle via scanline. Vertices in screen space, any winding.
    void _fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, lv_color_t c) {
        // Sort by y ascending
        if (y0 > y1) { int t = y0; y0 = y1; y1 = t; t = x0; x0 = x1; x1 = t; }
        if (y0 > y2) { int t = y0; y0 = y2; y2 = t; t = x0; x0 = x2; x2 = t; }
        if (y1 > y2) { int t = y1; y1 = y2; y2 = t; t = x1; x1 = x2; x2 = t; }
        if (y2 == y0) return;
        int totalH = y2 - y0;
        for (int i = 0; i < totalH; ++i) {
            bool second = (i > y1 - y0) || (y1 == y0);
            int segH = second ? (y2 - y1) : (y1 - y0);
            if (segH == 0) continue;
            float alpha = (float)i / (float)totalH;
            float beta  = (float)(i - (second ? y1 - y0 : 0)) / (float)segH;
            int ax = x0 + (int)((x2 - x0) * alpha);
            int bx = second ? (x1 + (int)((x2 - x1) * beta))
                            : (x0 + (int)((x1 - x0) * beta));
            if (ax > bx) { int t = ax; ax = bx; bx = t; }
            int y = y0 + i;
            if ((unsigned)y >= (unsigned)_h) continue;
            int x0c = ax < 0 ? 0 : ax;
            int x1c = (bx + 1 > _w) ? _w : (bx + 1);
            if (x0c >= x1c) continue;
            lv_color_t* row = _buf + y * _w;
            for (int x = x0c; x < x1c; ++x) row[x] = c;
        }
    }

    // -------- camera / projection --------
    float _focal() const {
        // Use the smaller viewport dimension so FOV stays consistent across
        // aspect ratios. Constant horizontal FOV of ~55deg in the smaller
        // axis: f = (s/2) / tan(fov/2) = (s/2) / tan(27.5deg).
        int s = (_w < _h) ? _w : _h;
        return (s * 0.5f) / 0.5206f;
    }

    void _buildViewProj() {
        // View = T(0,0,-d) * Rx(-pitch) * Ry(-yaw)
        _view = Mat4::translate(0, 0, -_camDist)
              * Mat4::rotateX(-_camPitch)
              * Mat4::rotateY(-_camYaw);
    }

    // Camera-space point -> integer pixel coords. False if behind the near
    // plane. The threshold here must match (or be looser than) the clip
    // epsilon used in _drawWorldLine so freshly clipped points still project.
    bool _project(const Vec3& p, int& sx, int& sy) const {
        if (p.z > -0.04f) return false;
        float f = _focal();
        float invZ = 1.0f / -p.z;
        sx = (int)(_w * 0.5f + p.x * f * invZ);
        sy = (int)(_h * 0.5f - p.y * f * invZ);
        return true;
    }

    // Grid lines on the y=0 world plane projected as line segments. We sample
    // the start/end of each segment in world space and let _project clip
    // anything behind the near plane.
    void _drawGrid() {
        const int half = 4;
        const float step = 0.5f;
        const lv_color_t cMinor = lv_color_hex(0x2A1F4A);
        const lv_color_t cMajor = lv_color_hex(0x6033A8);
        for (int i = -half; i <= half; ++i) {
            float a = i * step;
            lv_color_t c = (i == 0) ? cMajor : cMinor;
            Vec3 p0(a, 0, -half * step);
            Vec3 p1(a, 0,  half * step);
            Vec3 q0(-half * step, 0, a);
            Vec3 q1( half * step, 0, a);
            _drawWorldLine(p0, p1, c);
            _drawWorldLine(q0, q1, c);
        }
    }

    void _drawAxes() {
        Vec3 o(0, 0, 0);
        _drawWorldLine(o, Vec3(1, 0, 0), lv_color_hex(0xFF3060));
        _drawWorldLine(o, Vec3(0, 1, 0), lv_color_hex(0x60FF60));
        _drawWorldLine(o, Vec3(0, 0, 1), lv_color_hex(0x4080FF));
    }

    void _drawWorldLine(const Vec3& a, const Vec3& b, lv_color_t c) {
        Vec3 ca = _view.transform(a);
        Vec3 cb = _view.transform(b);
        // Clip the segment against z = -near so we never project bad values.
        const float nearZ = -0.05f;
        if (ca.z >= nearZ && cb.z >= nearZ) return;
        if (ca.z >= nearZ || cb.z >= nearZ) {
            float t = (nearZ - ca.z) / (cb.z - ca.z);
            Vec3 hit(
                ca.x + (cb.x - ca.x) * t,
                ca.y + (cb.y - ca.y) * t,
                nearZ);
            if (ca.z >= nearZ) ca = hit; else cb = hit;
        }
        int ax, ay, bx, by;
        if (!_project(ca, ax, ay) || !_project(cb, bx, by)) return;
        _drawLine(ax, ay, bx, by, c);
    }

    int _w, _h;
    lv_color_t* _buf = nullptr;
    lv_obj_t* _canvas = nullptr;

    Mat4 _view = Mat4::identity();
    float _camYaw = 0.6f;
    float _camPitch = 0.5f;
    float _camDist = 3.5f;
};
