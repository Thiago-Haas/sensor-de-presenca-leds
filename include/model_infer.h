#pragma once
#include <math.h>
#include "model.h"

// ── Funções auxiliares ───────────────────────────────────────────
static void relu(float* v, int n) {
    for (int i = 0; i < n; i++) if (v[i] < 0) v[i] = 0;
}

static int argmax(float* v, int n) {
    int best = 0;
    for (int i = 1; i < n; i++) if (v[i] > v[best]) best = i;
    return best;
}

// Multiplicação matriz-vetor: out[o] = Σ in[i]*W[i*out_n+o] + B[o]
static void dense(const float* in, int in_n,
                  const float* W,  const float* B, int out_n,
                  float* out) {
    for (int o = 0; o < out_n; o++) {
        out[o] = B[o];
        for (int i = 0; i < in_n; i++)
            out[o] += in[i] * W[i * out_n + o];
    }
}

// ── Inferência ───────────────────────────────────────────────────
// Retorna índice da pessoa (0-5) ou -1 se confiança < min_confidence
int predict_person(float height_cm, float min_confidence = 0.70f) {
    float x    = (height_cm - MODEL_MEAN) / MODEL_STD;
    float h1[16], h2[8], out[6];

    dense(&x, 1,  W0, B0, 16, h1); relu(h1, 16);  // Dense(16, relu)
    dense(h1, 16, W1, B1, 8,  h2); relu(h2, 8);   // Dense(8,  relu)
    dense(h2, 8,  W2, B2, 6,  out);                // Dense(6,  softmax)

    // Softmax
    float sum = 0;
    for (int i = 0; i < 6; i++) { out[i] = expf(out[i]); sum += out[i]; }
    for (int i = 0; i < 6; i++) out[i] /= sum;

    int best = argmax(out, 6);
    return (out[best] >= min_confidence) ? best : -1;
}