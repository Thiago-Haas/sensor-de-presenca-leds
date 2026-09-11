#pragma once
#include <math.h>
#include "model.h"

// Vizinho mais próximo com margem de confiança.
// Retorna índice da pessoa (0-7) ou -1 se:
//   - distância ao mais próximo > MATCH_MARGIN, ou
//   - dois candidatos estão dentro da margem (ambíguo)
int predict_person(float height_cm, float min_confidence = 0.70f) {
    (void)min_confidence;  // não usado nessa abordagem

    int   best_idx  = -1;
    float best_dist = 9999.0f;
    int   second_idx  = -1;
    float second_dist = 9999.0f;

    for (int i = 0; i < NUM_PESSOAS; i++) {
        float dist = fabsf(height_cm - ALTURAS[i]);
        if (dist < best_dist) {
            second_dist = best_dist; second_idx = best_idx;
            best_dist   = dist;      best_idx   = i;
        } else if (dist < second_dist) {
            second_dist = dist; second_idx = i;
        }
    }

    // Fora da margem → desconhecido
    if (best_dist > MATCH_MARGIN) return -1;

    // Dois candidatos dentro da margem → ambíguo → desconhecido
    if (second_dist <= MATCH_MARGIN) return -1;

    return best_idx;
}