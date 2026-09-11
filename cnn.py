import os
os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
os.environ['TF_ENABLE_ONEDNN_OPTS'] = '0'

import numpy as np

# ── Banco de alturas do lab ──────────────────────────────────────
pessoas = {
    "Thiago_Jacques": 182.0,
    "Andre_Herzfeld":  177.0,
    "Bruna_Henning":   164.0,
    "Thiago_Has":      203.0,
    "Eduardo":         183.0,
    "Ernani":          180.0,
    "Miguel":          176.0,
    "Larissa":         158.0,
}
nomes   = list(pessoas.keys())
alturas = list(pessoas.values())

# ── Calcula margem segura entre vizinhos ─────────────────────────
alturas_ord = sorted(zip(alturas, nomes))
print("\nAlturas ordenadas:")
for h, n in alturas_ord:
    print(f"  {n}: {h} cm")

print("\nDiferenças entre vizinhos:")
for i in range(1, len(alturas_ord)):
    diff = alturas_ord[i][0] - alturas_ord[i-1][0]
    print(f"  {alturas_ord[i-1][1]} → {alturas_ord[i][1]}: {diff:.1f} cm")

# ── Exporta como lookup table com margem de ±1.5 cm ─────────────
MARGEM = 2.5   # sensor HC-SR04 tem ~±2 cm; margem conservadora

with open("include/model.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write(f"constexpr float MATCH_MARGIN = {MARGEM:.1f}f;\n")
    f.write(f"constexpr int   NUM_PESSOAS  = {len(nomes)};\n\n")
    f.write("const char* LABELS[] = {" +
            ", ".join(f'"{n}"' for n in nomes) + "};\n\n")
    f.write("const float ALTURAS[] = {" +
            ", ".join(f"{h:.1f}f" for h in alturas) + "};\n")

print(f"\nExportado → include/model.h  (margem ±{MARGEM} cm, sem CNN)")
print("Abordagem: vizinho mais próximo com margem de confiança")