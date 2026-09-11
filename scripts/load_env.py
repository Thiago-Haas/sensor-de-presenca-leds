"""Gera configuracao (Wi-Fi, MQTT e calibracao) a partir do .env, sem colocar
segredos na linha de comando do compilador nem versionar valores locais."""
import json
from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR"))

# Cada chave define seu tipo C++ e o valor padrao usado quando ausente do .env.
STRING_DEFAULTS = {
    "WIFI_SSID": "",
    "WIFI_PASSWORD": "",
    "MQTT_HOST": "",
    "MQTT_USER": "",
    "MQTT_PASSWORD": "",
    "DEVICE_ID": "porta-01",
}
FLOAT_DEFAULTS = {
    "SENSOR_HEIGHT_CM": 216.0,
    "MIN_HEIGHT_CM": 50.0,
}
INT_DEFAULTS = {
    "MQTT_PORT": 1883,
    "LED_HOLD_MS": 2000,
}
ALL_KEYS = set(STRING_DEFAULTS) | set(FLOAT_DEFAULTS) | set(INT_DEFAULTS)

raw_values = {}
source = project / ".env"
if source.exists():
    for number, raw in enumerate(source.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator:
            raise ValueError(f".env: linha {number} precisa de CHAVE=valor")
        key, value = key.strip(), value.strip()
        if key not in ALL_KEYS:
            continue
        if value.startswith(('"', "'")):
            if len(value) < 2 or value[-1] != value[0]:
                raise ValueError(f".env: aspas incompletas na linha {number}")
            value = value[1:-1]
        raw_values[key] = value
# Nao interpreta comandos, variaveis nem escapes dentro dos valores.


def parse_float(key, value):
    try:
        return float(value)
    except ValueError:
        raise ValueError(f".env: {key} precisa ser numerico (recebido {value!r})")


def parse_int(key, value):
    try:
        return int(value)
    except ValueError:
        raise ValueError(f".env: {key} precisa ser um inteiro (recebido {value!r})")


lines = ["#pragma once", ""]
for key, default in STRING_DEFAULTS.items():
    value = raw_values.get(key, default)
    lines.append(f"constexpr char {key}[] = {json.dumps(value, ensure_ascii=False)};")
for key, default in FLOAT_DEFAULTS.items():
    value = parse_float(key, raw_values[key]) if key in raw_values else default
    lines.append(f"constexpr float {key} = {value}f;")
for key, default in INT_DEFAULTS.items():
    value = parse_int(key, raw_values[key]) if key in raw_values else default
    cpp_type = "unsigned short" if key == "MQTT_PORT" else "unsigned long"
    lines.append(f"constexpr {cpp_type} {key} = {value};")
content = "\n".join(lines) + "\n"

output = Path(env.subst("$BUILD_DIR")) / "generated"
output.mkdir(parents=True, exist_ok=True)
header = output / "env_config.h"
if not header.exists() or header.read_text(encoding="utf-8") != content:
    header.write_text(content, encoding="utf-8")
header.chmod(0o600)
env.Append(CPPPATH=[str(output)])
