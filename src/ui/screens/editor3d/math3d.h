#pragma once
#include <math.h>

// Minimal single-precision 3D math used by the editor.
// Right-handed, column-vector convention: out = M * v.

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
};

inline float vDot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 vCross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}
inline float vLen(const Vec3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
inline Vec3 vNorm(const Vec3& v) {
    float l = vLen(v);
    return l > 1e-6f ? Vec3(v.x / l, v.y / l, v.z / l) : Vec3(0, 0, 0);
}

// 4x4 matrix, row-major storage (m[row*4 + col]).
struct Mat4 {
    float m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }
    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[3] = x; r.m[7] = y; r.m[11] = z;
        return r;
    }
    static Mat4 scale(float x, float y, float z) {
        Mat4 r{};
        r.m[0] = x; r.m[5] = y; r.m[10] = z; r.m[15] = 1.0f;
        return r;
    }
    static Mat4 rotateX(float a) {
        float c = cosf(a), s = sinf(a);
        Mat4 r = identity();
        r.m[5] = c; r.m[6] = -s;
        r.m[9] = s; r.m[10] = c;
        return r;
    }
    static Mat4 rotateY(float a) {
        float c = cosf(a), s = sinf(a);
        Mat4 r = identity();
        r.m[0] = c;  r.m[2] = s;
        r.m[8] = -s; r.m[10] = c;
        return r;
    }
    static Mat4 rotateZ(float a) {
        float c = cosf(a), s = sinf(a);
        Mat4 r = identity();
        r.m[0] = c; r.m[1] = -s;
        r.m[4] = s; r.m[5] = c;
        return r;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) s += m[i * 4 + k] * o.m[k * 4 + j];
                r.m[i * 4 + j] = s;
            }
        }
        return r;
    }

    // Transform a point (implicit w=1).
    Vec3 transform(const Vec3& v) const {
        return Vec3(
            m[0] * v.x + m[1] * v.y + m[2]  * v.z + m[3],
            m[4] * v.x + m[5] * v.y + m[6]  * v.z + m[7],
            m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11]);
    }

    // Transform a direction (implicit w=0).
    Vec3 transformDir(const Vec3& v) const {
        return Vec3(
            m[0] * v.x + m[1] * v.y + m[2]  * v.z,
            m[4] * v.x + m[5] * v.y + m[6]  * v.z,
            m[8] * v.x + m[9] * v.y + m[10] * v.z);
    }
};
