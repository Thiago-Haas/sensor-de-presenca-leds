#!/bin/sh
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo "Execute este script com sudo."
    exit 1
fi
config=/etc/mosquitto/conf.d/sensor-alturas-lan.conf
if [ -e "$config" ]; then
    echo "O arquivo $config ja existe; revise-o antes de alterar."
    exit 1
fi
cat > "$config" <<'CONFIG'
# ESP32 e Node-RED na rede local confiavel; sem autenticacao neste prototipo.
listener 1883 127.0.0.1
listener 1883 10.120.34.240
allow_anonymous true
CONFIG
if ! systemctl restart mosquitto; then
    rm "$config"
    systemctl restart mosquitto
    echo "Falha na nova configuracao; configuracao anterior restaurada."
    exit 1
fi
systemctl is-active mosquitto
printf 'Broker disponivel em 10.120.34.240:1883.\n'
