// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Board's LED control

#ifndef BLINK_PERIOD
#define BLINK_PERIOD 500000
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // for ESP32 Dev Module
#endif

void setupLED() {
    pinMode(LED_BUILTIN, OUTPUT);
}

static bool ledManual = false;
static unsigned long manualStartTime = 0;
static const unsigned long MANUAL_TIMEOUT = 5000; // 5秒后恢复自动模式

void setLED(bool on) {
    static bool state = false;
    if (on == state) return;
    digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
    state = on;
    ledManual = true;
    manualStartTime = millis(); // 记录手动控制开始时间
}

void blinkLED() {
    if (ledManual) {
        // 检查是否超时，超时后恢复自动模式
        if (millis() - manualStartTime > MANUAL_TIMEOUT) {
            ledManual = false;
        } else {
            return;
        }
    }
    // 注意：这里应该使用 micros() 和 BLINK_PERIOD
    setLED(micros() / BLINK_PERIOD % 2);
}