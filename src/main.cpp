#include <Arduino.h>

constexpr uint8_t kLedPin = 18;
constexpr uint8_t kEchoPin = 2;
constexpr uint8_t kTriggerPin = 4;
constexpr float kDetectionDistanceCm = 80.0f;
constexpr unsigned long kMeasurementIntervalMs = 70;
constexpr unsigned long kEchoTimeoutUs = 30000;
constexpr unsigned long kLedHoldMs = 2000;

unsigned long lastMeasurementMs = 0;
unsigned long lastDetectionMs = 0;
bool ledOn = false;

void setup() {
    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, LOW);
    pinMode(kTriggerPin, OUTPUT);
    digitalWrite(kTriggerPin, LOW);
    pinMode(kEchoPin, INPUT);
    Serial.begin(115200);
    Serial.println("Detector de passagem iniciado: LED D18, Echo D2, Trigger D4.");
}

void loop() {
    const unsigned long now = millis();
    if (now - lastMeasurementMs >= kMeasurementIntervalMs) {
        lastMeasurementMs = now;
        digitalWrite(kTriggerPin, LOW);
        delayMicroseconds(2);
        digitalWrite(kTriggerPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(kTriggerPin, LOW);

        // Espera limitada: sensor desconectado nao bloqueia indefinidamente.
        const unsigned long echoUs = pulseIn(kEchoPin, HIGH, kEchoTimeoutUs);
        const float distanceCm = echoUs / 58.0f;
        // Timeout e leituras fora da faixa do sensor nao acionam o LED.
        const bool valid = echoUs != 0 && distanceCm >= 2.0f && distanceCm <= 400.0f;
        if (valid && distanceCm <= kDetectionDistanceCm) {
            lastDetectionMs = millis();
            if (!ledOn) {
                ledOn = true;
                digitalWrite(kLedPin, HIGH);
                Serial.println("Presenca detectada: LED ligado.");
            }
        }
        if (valid) {
            Serial.printf("Distancia: %.1f cm\n", distanceCm);
        } else {
            Serial.println("Sem leitura valida.");
        }
    }

    // Subtracao sem sinal mantem a temporizacao correta apos rollover de millis().
    if (ledOn && millis() - lastDetectionMs >= kLedHoldMs) {
        ledOn = false;
        digitalWrite(kLedPin, LOW);
        Serial.println("LED desligado.");
    }
}
