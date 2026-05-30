#pragma once
#include <stdint.h>
#include <string.h>
#include <lvgl.h>
#include "math3d.h"

// Mesh capacities. The editor cap is 40 edges (per the product brief). Vertex
// and face counts are chosen so that the cube/icosa/pyramid presets all fit
// without exceeding any limit.
static constexpr int MESH_MAX_VERTS = 40;
static constexpr int MESH_MAX_EDGES = 40;
static constexpr int MESH_MAX_FACES = 24;
static constexpr int MESH_PALETTE_SIZE = 5;

struct Edge {
    uint8_t a, b;       // vertex indices
    uint8_t color;      // palette index
};

struct Face {
    uint8_t a, b, c;    // vertex indices (CCW front)
    uint8_t color;      // palette index
};

struct Mesh {
    // Transform
    Vec3 position{ 0.0f, 0.0f, 0.0f };
    Vec3 rotation{ 0.0f, 0.0f, 0.0f }; // Euler radians (Y-X-Z order)
    Vec3 scale{ 1.0f, 1.0f, 1.0f };

    // Geometry
    Vec3 verts[MESH_MAX_VERTS];
    Edge edges[MESH_MAX_EDGES];
    Face faces[MESH_MAX_FACES];
    uint8_t vertCount = 0;
    uint8_t edgeCount = 0;
    uint8_t faceCount = 0;

    // Per-model palette (5 flat colors). The default is a vintage neon set.
    lv_color_t palette[MESH_PALETTE_SIZE];

    Mesh() {
        palette[0] = lv_color_hex(0xFF2D95); // magenta
        palette[1] = lv_color_hex(0x00E5FF); // cyan
        palette[2] = lv_color_hex(0xFFD400); // yellow
        palette[3] = lv_color_hex(0x39FF14); // neon green
        palette[4] = lv_color_hex(0xFF8C1A); // sunset orange
    }

    void clear() {
        vertCount = 0;
        edgeCount = 0;
        faceCount = 0;
        position = Vec3();
        rotation = Vec3();
        scale = Vec3(1, 1, 1);
    }

    int addVertex(const Vec3& p) {
        if (vertCount >= MESH_MAX_VERTS) return -1;
        verts[vertCount] = p;
        return vertCount++;
    }

    int addEdge(int a, int b, uint8_t color = 1) {
        if (a < 0 || b < 0 || a >= vertCount || b >= vertCount || a == b) return -1;
        if (edgeCount >= MESH_MAX_EDGES) return -1;
        // Reject duplicates (order-independent).
        for (uint8_t i = 0; i < edgeCount; ++i) {
            if ((edges[i].a == a && edges[i].b == b) ||
                (edges[i].a == b && edges[i].b == a)) return -1;
        }
        edges[edgeCount] = { (uint8_t)a, (uint8_t)b, color };
        return edgeCount++;
    }

    int addFace(int a, int b, int c, uint8_t color = 0) {
        if (a < 0 || b < 0 || c < 0) return -1;
        if (a >= vertCount || b >= vertCount || c >= vertCount) return -1;
        if (faceCount >= MESH_MAX_FACES) return -1;
        if (a == b || b == c || a == c) return -1;
        faces[faceCount] = { (uint8_t)a, (uint8_t)b, (uint8_t)c, color };
        return faceCount++;
    }

    // Drop a vertex and any edge/face that referenced it. Remaining indices
    // are remapped so the geometry stays valid.
    void removeVertex(int idx) {
        if (idx < 0 || idx >= vertCount) return;
        for (int i = idx; i < vertCount - 1; ++i) verts[i] = verts[i + 1];
        --vertCount;
        // Rebuild edges
        uint8_t outE = 0;
        for (uint8_t i = 0; i < edgeCount; ++i) {
            Edge e = edges[i];
            if (e.a == idx || e.b == idx) continue;
            if (e.a > idx) e.a--;
            if (e.b > idx) e.b--;
            edges[outE++] = e;
        }
        edgeCount = outE;
        // Rebuild faces
        uint8_t outF = 0;
        for (uint8_t i = 0; i < faceCount; ++i) {
            Face f = faces[i];
            if (f.a == idx || f.b == idx || f.c == idx) continue;
            if (f.a > idx) f.a--;
            if (f.b > idx) f.b--;
            if (f.c > idx) f.c--;
            faces[outF++] = f;
        }
        faceCount = outF;
    }

    void removeEdge(int idx) {
        if (idx < 0 || idx >= edgeCount) return;
        for (int i = idx; i < edgeCount - 1; ++i) edges[i] = edges[i + 1];
        --edgeCount;
    }

    void removeFace(int idx) {
        if (idx < 0 || idx >= faceCount) return;
        for (int i = idx; i < faceCount - 1; ++i) faces[i] = faces[i + 1];
        --faceCount;
    }

    Mat4 modelMatrix() const {
        return Mat4::translate(position.x, position.y, position.z)
             * Mat4::rotateY(rotation.y)
             * Mat4::rotateX(rotation.x)
             * Mat4::rotateZ(rotation.z)
             * Mat4::scale(scale.x, scale.y, scale.z);
    }
};

// ---------------------------------------------------------------------------
// Presets. Built so the cap budgets are not blown.
// ---------------------------------------------------------------------------
namespace mesh_presets {

inline void buildCube(Mesh& m, float s = 0.8f) {
    m.clear();
    // 8 corners
    m.addVertex(Vec3(-s, -s, -s)); // 0
    m.addVertex(Vec3( s, -s, -s)); // 1
    m.addVertex(Vec3( s,  s, -s)); // 2
    m.addVertex(Vec3(-s,  s, -s)); // 3
    m.addVertex(Vec3(-s, -s,  s)); // 4
    m.addVertex(Vec3( s, -s,  s)); // 5
    m.addVertex(Vec3( s,  s,  s)); // 6
    m.addVertex(Vec3(-s,  s,  s)); // 7
    // 12 edges
    const uint8_t e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i) m.addEdge(e[i][0], e[i][1], 1);
    // 12 face-triangles (2 per side). Each side a different palette color
    // cycled across 5 so the cube reads as classic shaded.
    const uint8_t f[12][3] = {
        {0,2,1},{0,3,2},       // back  -Z
        {4,5,6},{4,6,7},       // front +Z
        {1,2,6},{1,6,5},       // right +X
        {0,4,7},{0,7,3},       // left  -X
        {3,7,6},{3,6,2},       // top   +Y
        {0,1,5},{0,5,4}        // bottom-Y
    };
    const uint8_t col[12] = {0,0, 1,1, 2,2, 3,3, 4,4, 0,0};
    for (int i = 0; i < 12; ++i) m.addFace(f[i][0], f[i][1], f[i][2], col[i]);
}

inline void buildPyramid(Mesh& m, float s = 0.9f) {
    m.clear();
    m.addVertex(Vec3(-s, -s, -s)); // 0
    m.addVertex(Vec3( s, -s, -s)); // 1
    m.addVertex(Vec3( s, -s,  s)); // 2
    m.addVertex(Vec3(-s, -s,  s)); // 3
    m.addVertex(Vec3( 0,  s,  0)); // 4 apex
    // base + 4 slant edges
    const uint8_t e[8][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {0,4},{1,4},{2,4},{3,4}
    };
    for (int i = 0; i < 8; ++i) m.addEdge(e[i][0], e[i][1], 2);
    // sides
    m.addFace(0, 4, 1, 0);
    m.addFace(1, 4, 2, 1);
    m.addFace(2, 4, 3, 2);
    m.addFace(3, 4, 0, 3);
    // base
    m.addFace(0, 1, 2, 4);
    m.addFace(0, 2, 3, 4);
}

inline void buildPlane(Mesh& m, float s = 1.0f) {
    m.clear();
    m.addVertex(Vec3(-s, 0, -s));
    m.addVertex(Vec3( s, 0, -s));
    m.addVertex(Vec3( s, 0,  s));
    m.addVertex(Vec3(-s, 0,  s));
    m.addEdge(0, 1, 1);
    m.addEdge(1, 2, 1);
    m.addEdge(2, 3, 1);
    m.addEdge(3, 0, 1);
    m.addFace(0, 1, 2, 0);
    m.addFace(0, 2, 3, 0);
}

inline void buildDiamond(Mesh& m, float s = 0.9f) {
    m.clear();
    m.addVertex(Vec3( 0,  s, 0)); // 0 top
    m.addVertex(Vec3( s,  0, 0)); // 1
    m.addVertex(Vec3( 0,  0, s)); // 2
    m.addVertex(Vec3(-s,  0, 0)); // 3
    m.addVertex(Vec3( 0,  0,-s)); // 4
    m.addVertex(Vec3( 0, -s, 0)); // 5 bottom
    const uint8_t e[12][2] = {
        {0,1},{0,2},{0,3},{0,4},
        {1,2},{2,3},{3,4},{4,1},
        {5,1},{5,2},{5,3},{5,4}
    };
    for (int i = 0; i < 12; ++i) m.addEdge(e[i][0], e[i][1], 3);
    m.addFace(0, 2, 1, 0);
    m.addFace(0, 3, 2, 1);
    m.addFace(0, 4, 3, 2);
    m.addFace(0, 1, 4, 3);
    m.addFace(5, 1, 2, 4);
    m.addFace(5, 2, 3, 0);
    m.addFace(5, 3, 4, 1);
    m.addFace(5, 4, 1, 2);
}

} // namespace mesh_presets
