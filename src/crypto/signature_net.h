#pragma once
#include <Arduino.h>
#include <math.h>
#include "signature_features.h"
#include "signature_weights.h"

// SignatureNet - tiny MLP that maps a 40-dim feature vector into an
// 8-dim embedding.
namespace SignatureNet {
    static const int IN_DIM = SignatureFeatures::FEATURE_DIM;
    static const int HIDDEN_DIM = 16;
    static const int EMB_DIM = 8;

    // Forward pass. features[IN_DIM] -> embedding[EMB_DIM].
    inline void forward(const float* features, float* embedding) {
        float hidden[HIDDEN_DIM];
        for (int j = 0; j < HIDDEN_DIM; ++j) {
            float acc = SIG_W1_BIAS[j];
            for (int i = 0; i < IN_DIM; ++i) {
                acc += features[i] * SIG_W1[j * IN_DIM + i];
            }
            hidden[j] = acc > 0.0f ? acc : 0.0f;
        }
        for (int j = 0; j < EMB_DIM; ++j) {
            float acc = SIG_W2_BIAS[j];
            for (int i = 0; i < HIDDEN_DIM; ++i) {
                acc += hidden[i] * SIG_W2[j * HIDDEN_DIM + i];
            }
            embedding[j] = acc;
        }
        float norm = 0.0f;
        for (int j = 0; j < EMB_DIM; ++j) norm += embedding[j] * embedding[j];
        norm = sqrtf(norm);
        if (norm < 1e-6f) norm = 1.0f;
        for (int j = 0; j < EMB_DIM; ++j) embedding[j] /= norm;
    }

    // Euclidean distance between two embeddings.
    inline float distance(const float* a, const float* b) {
        float s = 0.0f;
        for (int i = 0; i < EMB_DIM; ++i) {
            float d = a[i] - b[i];
            s += d * d;
        }
        return sqrtf(s);
    }
}
