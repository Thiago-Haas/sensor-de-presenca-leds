"""Gera configuracao Wi-Fi sem colocar credenciais na linha do compilador."""
import json
from pathlib import Path

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
values = {"WIFI_SSID": "", "WIFI_PASSWORD": ""}
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
        if key not in values:
            continue
        if value.startswith(('"', "'")):
            if len(value) < 2 or value[-1] != value[0]:
                raise ValueError(f".env: aspas incompletas na linha {number}")
            value = value[1:-1]
        values[key] = value

# Nao interpreta comandos, variaveis nem escapes dentro dos valores.
output = Path(env.subst("$BUILD_DIR")) / "generated"
output.mkdir(parents=True, exist_ok=True)
header = output / "wifi_env.h"
content = "#pragma once\n" + "".join(
    f"constexpr char {key}[] = {json.dumps(value, ensure_ascii=False)};\n"
    for key, value in values.items()
)
if not header.exists() or header.read_text(encoding="utf-8") != content:
    header.write_text(content, encoding="utf-8")
header.chmod(0o600)
env.Append(CPPPATH=[str(output)])
