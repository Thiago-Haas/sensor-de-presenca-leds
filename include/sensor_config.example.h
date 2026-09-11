#pragma once
// OBSOLETO: os parametros de MQTT e calibracao agora vem do .env na raiz do
// projeto (gerados em env_config.h por scripts/load_env.py). Este arquivo nao
// e mais incluido por src/main.cpp; mantido apenas como referencia dos nomes
// e tipos originais. Veja a secao "Configuracao local" do README.
constexpr char MQTT_HOST[] = ""; // IP do computador na rede, nao localhost
constexpr unsigned short MQTT_PORT = 1883;
constexpr char MQTT_USER[] = "";
constexpr char MQTT_PASSWORD[] = "";
constexpr char DEVICE_ID[] = "porta-01"; // Letras, numeros e hifen; unico por ESP32
constexpr float SENSOR_HEIGHT_CM = 216.0f; // OBRIGATORIO: sensor ate o chao
constexpr float MIN_HEIGHT_CM = 50.0f;
constexpr unsigned long LED_HOLD_MS = 2000;
