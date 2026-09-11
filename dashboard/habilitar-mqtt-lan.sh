#!/bin/sh
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo "Execute este script com sudo."
    exit 1
fi
config=/etc/mosquitto/conf.d/sensor-alturas-lan.conf
backup=$(mktemp)
existed=0
if [ -e "$config" ]; then
    cp "$config" "$backup"
    existed=1
fi
trap 'rm -f "$backup"' EXIT
cat > "$config" <<'CONFIG'
# Prototipo na rede local confiavel; nao encaminhe a porta 1883 na internet.
# Escuta IPv4 incluindo localhost, sem depender do IP atribuido pelo celular.
listener 1883 0.0.0.0
allow_anonymous true
CONFIG
if ! systemctl restart mosquitto; then
    if [ "$existed" -eq 1 ]; then cp "$backup" "$config"; else rm -f "$config"; fi
    systemctl restart mosquitto
    echo "Falha na nova configuracao; configuracao anterior restaurada."
    exit 1
fi
systemctl is-active mosquitto
printf 'Broker ativo na porta 1883. IPs desta maquina: '
hostname -I
