#pragma once
#include <Preferences.h>
#include "signature_net.h"

// SignatureStore - persists the enrolled signature template in NVS.
// Namespace: "pickle-sig"
//   key "emb"  -> raw bytes of float[EMB_DIM]
//   key "thr"  -> float threshold

namespace SignatureStore {
    static const float DEFAULT_MARGIN = 1.6f;

    inline bool hasTemplate() {
        Preferences prefs;
        prefs.begin("pickle-sig", true);
        bool exists = prefs.isKey("emb") && prefs.isKey("thr");
        prefs.end();
        return exists;
    }

    inline void save(const float* centroid, float threshold) {
        Preferences prefs;
        prefs.begin("pickle-sig", false);
        prefs.putBytes("emb", centroid, SignatureNet::EMB_DIM * sizeof(float));
        prefs.putFloat("thr", threshold);
        prefs.end();
    }

    inline bool load(float* centroid, float* threshold) {
        Preferences prefs;
        prefs.begin("pickle-sig", true);
        if (!prefs.isKey("emb") || !prefs.isKey("thr")) {
            prefs.end();
            return false;
        }
        size_t got = prefs.getBytes("emb", centroid, SignatureNet::EMB_DIM * sizeof(float));
        *threshold = prefs.getFloat("thr", 0.5f);
        prefs.end();
        return got == SignatureNet::EMB_DIM * sizeof(float);
    }

    inline void clear() {
        Preferences prefs;
        prefs.begin("pickle-sig", false);
        prefs.remove("emb");
        prefs.remove("thr");
        prefs.end();
    }

    // Build a template from N captured embeddings: centroid + threshold based
    // on the average distance from each sample to the centroid, multiplied
    // by a safety margin to tolerate natural variability.
    inline void buildTemplate(const float* embeddings, int count,
                              float* outCentroid, float* outThreshold,
                              float margin = DEFAULT_MARGIN) {
        for (int j = 0; j < SignatureNet::EMB_DIM; ++j) outCentroid[j] = 0.0f;
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < SignatureNet::EMB_DIM; ++j) {
                outCentroid[j] += embeddings[i * SignatureNet::EMB_DIM + j];
            }
        }
        for (int j = 0; j < SignatureNet::EMB_DIM; ++j) outCentroid[j] /= count;

        float sumDist = 0.0f;
        for (int i = 0; i < count; ++i) {
            sumDist += SignatureNet::distance(
                outCentroid,
                embeddings + i * SignatureNet::EMB_DIM);
        }
        float meanDist = (count > 0) ? (sumDist / count) : 0.0f;
        float thr = meanDist * margin;
        if (thr < 0.05f) thr = 0.05f;
        *outThreshold = thr;
    }
}
