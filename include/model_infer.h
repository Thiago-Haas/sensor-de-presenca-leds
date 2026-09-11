#pragma once
#include <math.h>
#include "model.h"

struct Predicao {
    int   idx1;       // índice do mais provável
    int   idx2;       // índice do segundo (-1 se não ambíguo)
    float prob1;      // probabilidade do mais provável (0.0 a 1.0)
    float prob2;      // probabilidade do segundo (0.0 se não ambíguo)
    bool  ambiguo;    // true se dois candidatos dentro da margem
};

// Vizinho mais próximo com retorno de candidatos quando ambíguo.
// Retorna struct com até 2 candidatos e suas probabilidades.
Predicao predict_person(float height_cm) {
    Predicao result = {-1, -1, 0.0f, 0.0f, false};

    int   best_idx    = -1;
    float best_dist   = 9999.0f;
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

    // Totalmente fora da margem → desconhecido
    if (best_dist > MATCH_MARGIN) {
        result.idx1 = -1;
        return result;
    }

    // Dois dentro da margem → ambíguo com probabilidades
    if (second_dist <= MATCH_MARGIN) {
        // Probabilidade inversa à distância
        float w1 = 1.0f / (best_dist   + 0.01f);
        float w2 = 1.0f / (second_dist + 0.01f);
        float total = w1 + w2;
        result.idx1    = best_idx;
        result.idx2    = second_idx;
        result.prob1   = w1 / total;
        result.prob2   = w2 / total;
        result.ambiguo = true;
        return result;
    }

    // Só um dentro da margem → certeza
    result.idx1    = best_idx;
    result.idx2    = -1;
    result.prob1   = 1.0f;
    result.prob2   = 0.0f;
    result.ambiguo = false;
    return result;
}