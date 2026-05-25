#pragma once
#include <Arduino.h>
#include <math.h>
#include <vector>

// SignatureFeatures - extracts a fixed length feature vector from a raw
// signature stroke buffer.
namespace SignatureFeatures {
    // One captured touch sample. Stroke is the index of the contiguous
    // pen down segment this point belongs to (0 for the first stroke,
    // incremented every time the pen lifts).
    struct Point {
        int16_t x;
        int16_t y;
        uint16_t t_ms;
        uint8_t stroke;
    };

    static const int FEATURE_DIM = 40;
    static const int DIR_BINS = 8;
    static const int CURV_BINS = 8;
    static const int VEL_BINS = 8;

    // Compute the feature vector. Output is written into out[FEATURE_DIM].
    // All features are normalized so they roughly live in [0, 1].
    inline void extract(const std::vector<Point>& pts, float* out) {
        for (int i = 0; i < FEATURE_DIM; ++i) out[i] = 0.0f;
        if (pts.size() < 4) return;

        const int n = (int)pts.size();
        int16_t minX = pts[0].x, maxX = pts[0].x;
        int16_t minY = pts[0].y, maxY = pts[0].y;
        uint8_t strokes = 0;
        for (const auto& p : pts) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
            if (p.stroke + 1 > strokes) strokes = p.stroke + 1;
        }
        float w = (float)(maxX - minX); if (w < 1.0f) w = 1.0f;
        float h = (float)(maxY - minY); if (h < 1.0f) h = 1.0f;
        float scale = (w > h) ? w : h;

        // Normalized coordinates (origin at bbox centre, scaled to unit box)
        std::vector<float> nx(n), ny(n);
        float cx = 0.0f, cy = 0.0f;
        for (int i = 0; i < n; ++i) {
            nx[i] = ((float)pts[i].x - minX) / scale;
            ny[i] = ((float)pts[i].y - minY) / scale;
            cx += nx[i];
            cy += ny[i];
        }
        cx /= n;
        cy /= n;

        // Variance of normalized coords
        float varX = 0.0f, varY = 0.0f;
        for (int i = 0; i < n; ++i) {
            varX += (nx[i] - cx) * (nx[i] - cx);
            varY += (ny[i] - cy) * (ny[i] - cy);
        }
        varX /= n;
        varY /= n;

        // Path length, durations, velocities
        float pathLen = 0.0f;
        float totalDur = (float)(pts[n - 1].t_ms - pts[0].t_ms);
        if (totalDur < 1.0f) totalDur = 1.0f;
        float penDownDur = 0.0f;
        float maxVel = 0.0f, sumVel = 0.0f;
        float prevVel = 0.0f, sumAcc = 0.0f;
        int velSamples = 0, accSamples = 0;
        float dirHist[DIR_BINS] = {0};
        float curvHist[CURV_BINS] = {0};
        float velHist[VEL_BINS] = {0};
        float prevAng = 0.0f;
        bool hasPrevAng = false;
        for (int i = 1; i < n; ++i) {
            float dx = nx[i] - nx[i - 1];
            float dy = ny[i] - ny[i - 1];
            float seg = sqrtf(dx * dx + dy * dy);
            pathLen += seg;
            if (pts[i].stroke == pts[i - 1].stroke) {
                float dt = (float)(pts[i].t_ms - pts[i - 1].t_ms);
                penDownDur += dt;
                if (dt < 1.0f) dt = 1.0f;
                float v = seg / (dt / 1000.0f);
                sumVel += v;
                if (v > maxVel) maxVel = v;
                velSamples++;

                float ang = atan2f(dy, dx);
                if (ang < 0) ang += 2.0f * (float)PI;
                int bin = (int)(ang / (2.0f * (float)PI) * DIR_BINS);
                if (bin < 0) bin = 0;
                if (bin >= DIR_BINS) bin = DIR_BINS - 1;
                dirHist[bin] += 1.0f;

                if (hasPrevAng) {
                    float da = ang - prevAng;
                    while (da > (float)PI) da -= 2.0f * (float)PI;
                    while (da < -(float)PI) da += 2.0f * (float)PI;
                    float cb = (fabsf(da) / (float)PI) * CURV_BINS;
                    int cbin = (int)cb;
                    if (cbin < 0) cbin = 0;
                    if (cbin >= CURV_BINS) cbin = CURV_BINS - 1;
                    curvHist[cbin] += 1.0f;
                }
                prevAng = ang;
                hasPrevAng = true;

                float vNorm = v / 5.0f;
                if (vNorm > 0.999f) vNorm = 0.999f;
                int vbin = (int)(vNorm * VEL_BINS);
                if (vbin < 0) vbin = 0;
                if (vbin >= VEL_BINS) vbin = VEL_BINS - 1;
                velHist[vbin] += 1.0f;

                if (velSamples > 1) {
                    sumAcc += fabsf(v - prevVel);
                    accSamples++;
                }
                prevVel = v;
            } else {
                hasPrevAng = false;
                prevVel = 0.0f;
            }
        }

        auto normalize = [](float* h, int bins) {
            float s = 0.0f;
            for (int i = 0; i < bins; ++i) s += h[i];
            if (s < 1e-6f) return;
            for (int i = 0; i < bins; ++i) h[i] /= s;
        };
        normalize(dirHist, DIR_BINS);
        normalize(curvHist, CURV_BINS);
        normalize(velHist, VEL_BINS);

        float meanVel = velSamples ? (sumVel / velSamples) : 0.0f;
        float meanAcc = accSamples ? (sumAcc / accSamples) : 0.0f;
        float aspect = (h > 0.0f) ? (w / h) : 0.0f;
        if (aspect > 4.0f) aspect = 4.0f;

        // Final feature vector
        out[0] = (float)strokes / 5.0f;
        out[1] = (totalDur / 1000.0f) / 10.0f;
        out[2] = pathLen / 5.0f;
        out[3] = aspect / 4.0f;
        out[4] = cx;
        out[5] = cy;
        out[6] = (float)n / 512.0f;
        out[7] = meanVel / 5.0f;
        out[8] = (maxVel / 10.0f);
        out[9] = meanAcc / 5.0f;
        out[10] = (float)(strokes - 1) / 5.0f;
        out[11] = (totalDur / n) / 50.0f;
        for (int i = 0; i < DIR_BINS; ++i) out[12 + i] = dirHist[i];
        for (int i = 0; i < CURV_BINS; ++i) out[20 + i] = curvHist[i];
        for (int i = 0; i < VEL_BINS; ++i) out[28 + i] = velHist[i];
        out[36] = penDownDur / totalDur;
        out[37] = varX * 4.0f;
        out[38] = varY * 4.0f;
        float diag = sqrtf(w * w + h * h);
        out[39] = (pathLen > 0.0f) ? (diag / scale / (pathLen + 1.0f)) : 0.0f;

        for (int i = 0; i < FEATURE_DIM; ++i) {
            if (!isfinite(out[i])) out[i] = 0.0f;
            if (out[i] > 4.0f) out[i] = 4.0f;
            if (out[i] < -4.0f) out[i] = -4.0f;
        }
    }
}
