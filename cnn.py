import numpy as np
import tensorflow as tf
from tensorflow import keras

# ── Banco de alturas do lab ──────────────────────────────────────
pessoas = {
    "Thiago_Jacques": 182.0,
    "Andre_Herzfeld":  177.0,
    "Bruna_Henning":   164.0,
    "Thiago_Has":      203.0,
    "Eduardo":         183.0,
    "Ernani":          180.0,
}
nomes   = list(pessoas.keys())
alturas = list(pessoas.values())

# ── Gera amostras com ruído realista do sensor ───────────────────
np.random.seed(42)
X, y = [], []
for i, h in enumerate(alturas):
    samples = np.random.normal(h, 1.0, 300)   # ← ruído ±1 cm
    X.extend(samples)
    y.extend([i] * 300)

X = np.array(X, dtype=np.float32).reshape(-1, 1, 1)
y = np.array(y, dtype=np.int32)

# ── Normalização ─────────────────────────────────────────────────
X_mean, X_std = X.mean(), X.std()
X_norm = (X - X_mean) / X_std

# ── Modelo ───────────────────────────────────────────────────────
model = keras.Sequential([
    keras.layers.Input(shape=(1, 1)),
    keras.layers.Dense(16, activation='relu'),
    keras.layers.Flatten(),
    keras.layers.Dense(8, activation='relu'),
    keras.layers.Dense(len(nomes), activation='softmax'),
])
model.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])
model.fit(X_norm, y, epochs=100, batch_size=32,
          validation_split=0.2, verbose=0)

loss, acc = model.evaluate(X_norm, y, verbose=0)
print(f"Acurácia final: {acc*100:.1f}%")

# ── Exporta pesos com índices sequenciais (0, 1, 2...) ───────────
def fmt_array(name, arr):
    flat = arr.flatten().tolist()
    vals = ", ".join(f"{v:.6f}f" for v in flat)
    return f"const float {name}[] = {{{vals}}};\n"

with open("include/model.h", "w") as f:
    f.write("#pragma once\n\n")
    f.write(f"constexpr float MODEL_MEAN = {X_mean:.4f}f;\n")
    f.write(f"constexpr float MODEL_STD  = {X_std:.4f}f;\n\n")
    f.write('const char* LABELS[] = {' +
            ', '.join(f'"{n}"' for n in nomes) + '};\n\n')

    layer_idx = 0
    for layer in model.layers:
        weights = layer.get_weights()
        if len(weights) == 2:
            f.write(fmt_array(f"W{layer_idx}", weights[0]))
            f.write(fmt_array(f"B{layer_idx}", weights[1]))
            f.write("\n")
            layer_idx += 1

print(f"Exportados {layer_idx} layers → include/model.h")