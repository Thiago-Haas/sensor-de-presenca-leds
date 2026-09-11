#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "wifi_env.h"
#if __has_include("sensor_config.h")
#include "sensor_config.h"
#else
#include "sensor_config.example.h"
#endif

constexpr uint8_t LED = 18, ECHO = 2, TRIGGER = 4;
struct Event { char payload[256]; };
struct Heartbeat { uint32_t uptimeMs; bool sensorOk; };
QueueHandle_t events, heartbeats;
char eventTopic[96], statusTopic[96], heartbeatTopic[96], clientId[80];
uint32_t bootId, sequence = 0;

// Rede em tarefa separada: reconexao nao interrompe as medidas.
void networkTask(void*) {
    WiFiClient transport;
    PubSubClient mqtt(transport);
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setSocketTimeout(2);
    WiFi.mode(WIFI_STA);
    uint32_t wifiRetry = millis() - 15000, mqttRetry = millis() - 5000;
    Event pending{};
    bool hasPending = false;
    for (;;) {
        const uint32_t now = millis();
        if (WIFI_SSID[0] && WiFi.status() != WL_CONNECTED && now - wifiRetry >= 15000) {
            wifiRetry = now;
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        if (WiFi.status() == WL_CONNECTED && MQTT_HOST[0]) {
            if (!mqtt.connected() && now - mqttRetry >= 5000) {
                mqttRetry = now;
                if (mqtt.connect(clientId, MQTT_USER, MQTT_PASSWORD,
                                 statusTopic, 1, true, "offline")) {
                    mqtt.publish(statusTopic, "online", true);
                }
            }
            if (mqtt.connected()) {
                mqtt.loop();
                Heartbeat heartbeat{};
                if (xQueueReceive(heartbeats, &heartbeat, 0) == pdTRUE &&
                    millis() - heartbeat.uptimeMs < 10000) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                        "{\"uptime_ms\":%lu,\"sensor_ok\":%s,\"rssi_dbm\":%d}",
                        (unsigned long)heartbeat.uptimeMs,
                        heartbeat.sensorOk ? "true" : "false", WiFi.RSSI());
                    mqtt.publish(heartbeatTopic, payload, false);
                }
                if (!hasPending) hasPending = xQueueReceive(events, &pending, 0) == pdTRUE;
                if (hasPending && mqtt.publish(eventTopic, pending.payload, false)) {
                    hasPending = false;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT); digitalWrite(LED, LOW);
    pinMode(TRIGGER, OUTPUT); digitalWrite(TRIGGER, LOW);
    pinMode(ECHO, INPUT);
    bootId = esp_random();
    snprintf(clientId, sizeof(clientId), "%s-%08lx", DEVICE_ID, (unsigned long)bootId);
    snprintf(eventTopic, sizeof(eventTopic), "porta/%s/altura", DEVICE_ID);
    snprintf(statusTopic, sizeof(statusTopic), "porta/%s/status", DEVICE_ID);
    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "porta/%s/heartbeat", DEVICE_ID);
    events = xQueueCreate(20, sizeof(Event));
    heartbeats = xQueueCreate(1, sizeof(Heartbeat));
    if (!events || !heartbeats || xTaskCreate(networkTask, "mqtt", 6144, nullptr, 1, nullptr) != pdPASS) {
        Serial.println("Falha ao iniciar MQTT. Reinicie a placa.");
        while (true) delay(1000);
    }
    Serial.println("Sensor D2/D4; LED D18. Altura requer sensor no alto, voltado ao chao.");
    if (SENSOR_HEIGHT_CM <= MIN_HEIGHT_CM || SENSOR_HEIGHT_CM > 400)
        Serial.println("Configure SENSOR_HEIGHT_CM em include/sensor_config.h. Alturas desativadas.");
}

void loop() {
    static uint32_t lastHeartbeat = millis() - 5000;
    static uint32_t lastRead = 0, lastSeen = 0, lastValid = 0, started = 0;
    static float window[3] = {}, peak = 0;
    static unsigned filled = 0, index = 0, clearSamples = 0, heightSamples = 0;
    static bool active = false, ledOn = false;
    uint32_t now = millis();
    // Gerado pela tarefa de medicao: travamento do loop interrompe o sinal de vida.
    if (now - lastHeartbeat >= 5000) {
        lastHeartbeat = now;
        Heartbeat heartbeat{now, lastValid != 0 && now - lastValid < 2000};
        xQueueOverwrite(heartbeats, &heartbeat);
    }
    if (ledOn && now - lastSeen >= LED_HOLD_MS) {
        ledOn = false; digitalWrite(LED, LOW);
    }
    if (now - lastRead < 70) return;
    lastRead = now;
    digitalWrite(TRIGGER, LOW); delayMicroseconds(2);
    digitalWrite(TRIGGER, HIGH); delayMicroseconds(10);
    digitalWrite(TRIGGER, LOW);
    const unsigned long pulse = pulseIn(ECHO, HIGH, 30000);
    const float distance = pulse / 58.0f;
    now = millis();
    if (!pulse || distance < 2 || distance > 400) {
        filled = index = clearSamples = 0;
        // Falta de eco nao prova que a pessoa saiu: descarta, nao inventa altura.
        if (active && now - lastValid > 2000) {
            active = false; peak = 0; heightSamples = 0;
            Serial.println("Passagem descartada: perda de eco.");
        }
        return;
    }
    lastValid = now;
    Serial.printf("Distancia: %.1f cm\n", distance);
    const bool calibrated = SENSOR_HEIGHT_CM > MIN_HEIGHT_CM && SENSOR_HEIGHT_CM <= 400;
    if (!calibrated) {
        // Preserva a deteccao simples enquanto a altura nao foi configurada.
        if (distance <= 80) {
            lastSeen = now; ledOn = true; digitalWrite(LED, HIGH);
        }
        return;
    }
    window[index] = distance; index = (index + 1) % 3;
    if (filled < 3) ++filled;
    if (filled < 3) return;
    float a = window[0], b = window[1], c = window[2];
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }
    const float height = SENSOR_HEIGHT_CM - b;
    if (height >= MIN_HEIGHT_CM) {
        lastSeen = now; ledOn = true; digitalWrite(LED, HIGH);
        if (!active) { active = true; peak = 0; heightSamples = 0; started = now; }
        if (height > peak) peak = height;
        ++heightSamples; clearSamples = 0;
    } else if (active) {
        // Exige quatro leituras proximas ao piso para encerrar a passagem.
        if (fabsf(b - SENSOR_HEIGHT_CM) <= 15) ++clearSamples;
        else clearSamples = 0;
        if (clearSamples >= 4) {
            if (heightSamples >= 3) {
                Event event{};
                snprintf(event.payload, sizeof(event.payload),
                    "{\"event_id\":\"%08lx-%lu\",\"height_cm\":%.1f,\"sensor_height_cm\":%.1f,"
                    "\"uptime_ms\":%lu,\"duration_ms\":%lu,\"estimated\":true}",
                    (unsigned long)bootId, (unsigned long)++sequence, peak, SENSOR_HEIGHT_CM,
                    (unsigned long)now, (unsigned long)(now - started));
                Serial.println(event.payload);
                if (xQueueSend(events, &event, 0) != pdTRUE)
                    Serial.println("Fila MQTT cheia: registro descartado.");
            }
            active = false; clearSamples = heightSamples = 0; peak = 0;
        }
    }
}
