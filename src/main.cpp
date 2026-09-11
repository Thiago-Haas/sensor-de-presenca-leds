#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include "calibration.h"
#include "wifi_env.h"
#if __has_include("sensor_config.h")
#include "sensor_config.h"
#else
#include "sensor_config.example.h"
#endif
#include "model_infer.h"   // Identificacao por altura (vizinho mais proximo)

constexpr uint8_t LED = 18, ECHO = 2, TRIGGER = 4, CALIBRATE_BUTTON = 19;
struct Event { char payload[384]; };   // suporta o payload de pessoa ambigua (dois candidatos)
struct Heartbeat { uint32_t uptimeMs; bool sensorOk; };
struct CalibrationState { float distanceCm; bool saved; char state[20]; };
QueueHandle_t events, heartbeats, calibrationStates;
float sensorHeightCm = SENSOR_HEIGHT_CM;
bool calibrationSaved = false;
CalibrationButton calibrationButton;
FloorCalibration floorCalibration;

void reportCalibration(const char* state) {
    CalibrationState snapshot{};
    snapshot.distanceCm = sensorHeightCm;
    snapshot.saved = calibrationSaved;
    snprintf(snapshot.state, sizeof(snapshot.state), "%s", state);
    xQueueOverwrite(calibrationStates, &snapshot);
}
char eventTopic[96], statusTopic[96], heartbeatTopic[96], calibrationTopic[96], clientId[80];
uint32_t bootId, sequence = 0;

// ── Tarefa de rede ───────────────────────────────────────────────
void networkTask(void*) {
    WiFiClient transport;
    PubSubClient mqtt(transport);
    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setSocketTimeout(2);
    mqtt.setKeepAlive(15);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    int lastWifiStatus = -1;
    uint32_t wifiRetry = millis() - 15000, mqttRetry = millis() - 5000;
    Event pending{};
    bool hasPending = false;
    uint32_t lastCalibrationPublish = millis() - 5000;
    for (;;) {
        const uint32_t now = millis();
        const int wifiStatus = WiFi.status();
        if (wifiStatus != lastWifiStatus) {
            lastWifiStatus = wifiStatus;
            if (wifiStatus == WL_CONNECTED) {
                Serial.printf("[WiFi] Conectado. IP ESP32: %s; broker: %s:%u\n",
                              WiFi.localIP().toString().c_str(), MQTT_HOST, MQTT_PORT);
            } else {
                Serial.printf("[WiFi] Sem conexao (estado %d).\n", wifiStatus);
            }
        }
        if (WIFI_SSID[0] && WiFi.status() != WL_CONNECTED && now - wifiRetry >= 15000) {
            wifiRetry = now;
            Serial.println("[WiFi] Tentando conectar a rede configurada no .env...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
        if (WiFi.status() == WL_CONNECTED && MQTT_HOST[0]) {
            if (!mqtt.connected() && now - mqttRetry >= 5000) {
                mqttRetry = now;
                Serial.printf("[MQTT] Conectando a %s:%u...\n", MQTT_HOST, MQTT_PORT);
                if (mqtt.connect(clientId, MQTT_USER, MQTT_PASSWORD,
                                 statusTopic, 1, true, "offline")) {
                    Serial.println("[MQTT] Conectado ao broker.");
                    mqtt.publish(statusTopic, "online", true);
                    lastCalibrationPublish = millis() - 5000;
                } else {
                    Serial.printf("[MQTT] Falha, estado %d. Confira IP do broker, porta e firewall.\n", mqtt.state());
                }
            }
            if (mqtt.connected()) {
                mqtt.loop();
                CalibrationState calibration{};
                if (millis() - lastCalibrationPublish >= 1000 &&
                    xQueuePeek(calibrationStates, &calibration, 0) == pdTRUE) {
                    char payload[160];
                    snprintf(payload, sizeof(payload),
                        "{\"distance_cm\":%.1f,\"saved\":%s,\"state\":\"%s\"}",
                        calibration.distanceCm, calibration.saved ? "true" : "false", calibration.state);
                    if (mqtt.publish(calibrationTopic, payload, true))
                        lastCalibrationPublish = millis();
                }
                Heartbeat heartbeat{};
                if (xQueueReceive(heartbeats, &heartbeat, 0) == pdTRUE &&
                    millis() - heartbeat.uptimeMs < 10000) {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                        "{\"uptime_ms\":%lu,\"sensor_ok\":%s,\"rssi_dbm\":%d}",
                        (unsigned long)heartbeat.uptimeMs,
                        heartbeat.sensorOk ? "true" : "false", WiFi.RSSI());
                    if (mqtt.publish(heartbeatTopic, payload, false))
                        Serial.println("[MQTT] Sinal de vida enviado.");
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

// ── Setup ────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("Firmware: heartbeat + calibracao D19 + identificacao por altura + diagnostico de rede.");
    if (!WIFI_SSID[0]) Serial.println("[WiFi] Falta WIFI_SSID no .env.");
    if (!MQTT_HOST[0]) Serial.println("[MQTT] Falta MQTT_HOST em sensor_config.h.");
    pinMode(LED, OUTPUT);     digitalWrite(LED, LOW);
    pinMode(TRIGGER, OUTPUT); digitalWrite(TRIGGER, LOW);
    pinMode(ECHO, INPUT);
    pinMode(CALIBRATE_BUTTON, INPUT_PULLUP);
    Preferences prefs;
    if (prefs.begin("floor-cal", true)) {
        const float saved = prefs.getFloat("height", SENSOR_HEIGHT_CM);
        if (prefs.isKey("height") && isfinite(saved) && saved > MIN_HEIGHT_CM && saved <= 400) {
            sensorHeightCm = saved;
            calibrationSaved = true;
        }
        prefs.end();
    }
    bootId = esp_random();
    snprintf(clientId,    sizeof(clientId),    "%s-%08lx", DEVICE_ID, (unsigned long)bootId);
    snprintf(eventTopic,  sizeof(eventTopic),  "porta/%s/altura", DEVICE_ID);
    snprintf(statusTopic, sizeof(statusTopic), "porta/%s/status", DEVICE_ID);
    snprintf(heartbeatTopic, sizeof(heartbeatTopic), "porta/%s/heartbeat", DEVICE_ID);
    snprintf(calibrationTopic, sizeof(calibrationTopic), "porta/%s/calibration", DEVICE_ID);
    events = xQueueCreate(20, sizeof(Event));
    heartbeats = xQueueCreate(1, sizeof(Heartbeat));
    calibrationStates = xQueueCreate(1, sizeof(CalibrationState));
    if (calibrationStates) reportCalibration(calibrationSaved ? "saved" : "default");
    if (!events || !heartbeats || !calibrationStates || xTaskCreate(networkTask, "mqtt", 6144, nullptr, 1, nullptr) != pdPASS) {
        Serial.println("Falha ao iniciar MQTT. Reinicie a placa.");
        while (true) delay(1000);
    }
    Serial.println("Sensor D2/D4; LED D18. Altura requer sensor no alto, voltado ao chao.");
    if (sensorHeightCm <= MIN_HEIGHT_CM || sensorHeightCm > 400)
        Serial.println("Configure SENSOR_HEIGHT_CM em include/sensor_config.h. Alturas desativadas.");
}

// ── Loop principal ───────────────────────────────────────────────
void loop() {
    static uint32_t lastHeartbeat = millis() - 5000;
    static uint32_t lastRead = 0, lastSeen = 0, lastValid = 0, started = 0;
    static float window[3] = {}, peak = 0;
    static unsigned filled = 0, index = 0, clearSamples = 0, heightSamples = 0;
    static bool active = false, ledOn = false;

    uint32_t now = millis();
    if (calibrationButton.pressed(digitalRead(CALIBRATE_BUTTON) == LOW, now) &&
        !floorCalibration.active()) {
        floorCalibration.start(now);
        active = false; peak = 0; heightSamples = clearSamples = filled = index = 0;
        ledOn = false; digitalWrite(LED, LOW);
        reportCalibration("measuring");
        Serial.println("Calibrando: mantenha a passagem livre.");
    }
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

    digitalWrite(TRIGGER, LOW);  delayMicroseconds(2);
    digitalWrite(TRIGGER, HIGH); delayMicroseconds(10);
    digitalWrite(TRIGGER, LOW);
    const unsigned long pulse    = pulseIn(ECHO, HIGH, 30000);
    const float         distance = pulse / 58.0f;
    now = millis();

    if (floorCalibration.active()) {
        const bool valid = pulse != 0 && distance >= 2 && distance <= 400;
        if (valid) lastValid = now;
        float candidate = sensorHeightCm;
        const auto result = floorCalibration.sample(distance, valid, now, MIN_HEIGHT_CM, candidate);
        if (result == FloorCalibration::Ready) {
            Preferences prefs;
            bool stored = false;
            if (prefs.begin("floor-cal", false)) {
                stored = prefs.putFloat("height", candidate) == sizeof(float);
                prefs.end();
            }
            if (stored) {
                sensorHeightCm = candidate;
                calibrationSaved = true;
                reportCalibration("saved");
                Serial.printf("Distancia ao chao calibrada: %.1f cm\n", sensorHeightCm);
            } else {
                reportCalibration("save_failed");
                Serial.println("Falha ao salvar calibracao; valor anterior mantido.");
            }
        } else if (result == FloorCalibration::Unstable || result == FloorCalibration::NoEcho) {
            reportCalibration(result == FloorCalibration::Unstable ? "unstable" : "no_echo");
            Serial.println("Calibracao rejeitada; valor anterior mantido.");
        }
        // Nao classifica passagens durante a calibracao.
        return;
    }
    if (!pulse || distance < 2 || distance > 400) {
        filled = index = clearSamples = 0;
        if (active && now - lastValid > 2000) {
            active = false; peak = 0; heightSamples = 0;
            Serial.println("Passagem descartada: perda de eco.");
        }
        return;
    }
    lastValid = now;
    Serial.printf("Distancia: %.1f cm\n", distance);

    const bool calibrated = sensorHeightCm > MIN_HEIGHT_CM && sensorHeightCm <= 400;
    if (!calibrated) {
        if (distance <= 80) { lastSeen = now; ledOn = true; digitalWrite(LED, HIGH); }
        return;
    }

    window[index] = distance; index = (index + 1) % 3;
    if (filled < 3) ++filled;
    if (filled < 3) return;

    // Mediana de 3 amostras
    float a = window[0], b = window[1], c = window[2];
    if (a > b) { float t = a; a = b; b = t; }
    if (b > c) { float t = b; b = c; c = t; }
    if (a > b) { float t = a; a = b; b = t; }

    const float height = sensorHeightCm - b;

    if (height >= MIN_HEIGHT_CM) {
        lastSeen = now; ledOn = true; digitalWrite(LED, HIGH);
        if (!active) { active = true; peak = 0; heightSamples = 0; started = now; }
        if (height > peak) peak = height;
        ++heightSamples; clearSamples = 0;

    } else if (active) {
        if (fabsf(b - sensorHeightCm) <= 15) ++clearSamples;
        else clearSamples = 0;

        if (clearSamples >= 4) {
            if (heightSamples >= 3) {

                // ── Identificação por altura ──────────────────────
                Predicao p = predict_person(peak);
                Event event{};

                if (p.idx1 == -1) {
                    // Totalmente desconhecido
                    Serial.printf("Pessoa: desconhecido (pico %.1f cm)\n", peak);
                    snprintf(event.payload, sizeof(event.payload),
                        "{\"event_id\":\"%08lx-%lu\","
                        "\"height_cm\":%.1f,"
                        "\"person\":\"desconhecido\","
                        "\"sensor_height_cm\":%.1f,"
                        "\"uptime_ms\":%lu,"
                        "\"duration_ms\":%lu,"
                        "\"estimated\":true}",
                        (unsigned long)bootId, (unsigned long)++sequence,
                        peak,
                        sensorHeightCm,
                        (unsigned long)now,
                        (unsigned long)(now - started));

                } else if (p.ambiguo) {
                    // Ambíguo: dois candidatos com probabilidades
                    Serial.printf("Pessoa: ambiguo -> %s(%.0f%%) ou %s(%.0f%%) (pico %.1f cm)\n",
                        LABELS[p.idx1], p.prob1 * 100,
                        LABELS[p.idx2], p.prob2 * 100, peak);
                    snprintf(event.payload, sizeof(event.payload),
                        "{\"event_id\":\"%08lx-%lu\","
                        "\"height_cm\":%.1f,"
                        "\"person\":\"ambiguo\","
                        "\"candidatos\":["
                        "{\"nome\":\"%s\",\"prob\":%.2f},"
                        "{\"nome\":\"%s\",\"prob\":%.2f}],"
                        "\"sensor_height_cm\":%.1f,"
                        "\"uptime_ms\":%lu,"
                        "\"duration_ms\":%lu,"
                        "\"estimated\":true}",
                        (unsigned long)bootId, (unsigned long)++sequence,
                        peak,
                        LABELS[p.idx1], p.prob1,
                        LABELS[p.idx2], p.prob2,
                        sensorHeightCm,
                        (unsigned long)now,
                        (unsigned long)(now - started));

                } else {
                    // Identificado com certeza
                    Serial.printf("Pessoa: %s (pico %.1f cm)\n", LABELS[p.idx1], peak);
                    snprintf(event.payload, sizeof(event.payload),
                        "{\"event_id\":\"%08lx-%lu\","
                        "\"height_cm\":%.1f,"
                        "\"person\":\"%s\","
                        "\"sensor_height_cm\":%.1f,"
                        "\"uptime_ms\":%lu,"
                        "\"duration_ms\":%lu,"
                        "\"estimated\":true}",
                        (unsigned long)bootId, (unsigned long)++sequence,
                        peak,
                        LABELS[p.idx1],
                        sensorHeightCm,
                        (unsigned long)now,
                        (unsigned long)(now - started));
                }
                // ─────────────────────────────────────────────────

                Serial.println(event.payload);
                if (xQueueSend(events, &event, 0) != pdTRUE)
                    Serial.println("Fila MQTT cheia: registro descartado.");
            }
            active = false; clearSamples = heightSamples = 0; peak = 0;
        }
    }
}
