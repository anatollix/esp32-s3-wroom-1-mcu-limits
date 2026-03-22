#include <Arduino.h>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct Config {
    static constexpr uint8_t  LED_PIN            = 5;
    static constexpr uint8_t  BUTTON_PIN         = 9;
    static constexpr uint32_t BLINK_INTERVAL     = 500; // ms
    static constexpr uint32_t LOG_EVERY_N_ITERS  = 1000;
    static constexpr uint32_t BUTTON_DEBOUNCE_MS = 50; // ms
};

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
enum class LedState { On, Off };
enum class LedMode  { Blinking, AlwaysOn, AlwaysOff };

class Led {
public:
    void init() {
        pinMode(Config::LED_PIN, OUTPUT);
        set(LedState::Off);
    }

    void set(LedState state) {
        _state = state;
        digitalWrite(Config::LED_PIN, state == LedState::On ? HIGH : LOW);
    }

    void toggle() {
        set(_state == LedState::On ? LedState::Off : LedState::On);
    }

    LedState state() const { return _state; }

private:
    LedState _state = LedState::Off;
};

static const char* ledModeStr(LedMode mode) {
    switch (mode) {
        case LedMode::Blinking:  return "Blinking";
        case LedMode::AlwaysOn:  return "Always ON";
        case LedMode::AlwaysOff: return "Always OFF";
        default:                 return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static Led            led;
static LedMode        ledMode       = LedMode::Blinking;
static volatile bool  buttonPressed = false;

// ---------------------------------------------------------------------------
// ISR
// ---------------------------------------------------------------------------
void IRAM_ATTR buttonISR() {
    static uint32_t lastMs = 0;
    const uint32_t  now    = millis();
    if (now - lastMs >= Config::BUTTON_DEBOUNCE_MS) {
        buttonPressed = true;
        lastMs        = now;
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("\n========================================");
    Serial.println("   ESP32-S3 MCU Limits");
    Serial.println("========================================");
    Serial.printf("CPU Freq : %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("========================================\n");

    led.init();
    Serial.printf("Mode: %s\n", ledModeStr(ledMode));

    // external button with built-in pull-down
    pinMode(Config::BUTTON_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(Config::BUTTON_PIN), buttonISR, RISING);
}

// ---------------------------------------------------------------------------
// Superloop
// ---------------------------------------------------------------------------
void loop() {
    static uint32_t lastBlink   = 0;
    static uint32_t iterCount   = 0;
    static uint32_t lastLogTime = 0;

    const uint32_t now = millis();

    // --- Button: Blinking -> AlwaysOn -> AlwaysOff -> Blinking ---
    if (buttonPressed) {
        buttonPressed = false;
        switch (ledMode) {
            case LedMode::Blinking:
                ledMode = LedMode::AlwaysOn;
                led.set(LedState::On);
                break;
            case LedMode::AlwaysOn:
                ledMode = LedMode::AlwaysOff;
                led.set(LedState::Off);
                break;
            case LedMode::AlwaysOff:
                ledMode = LedMode::Blinking;
                break;
        }
        Serial.printf("Mode: %s\n", ledModeStr(ledMode));
    }

    // --- Non-blocking blink ---
    if (ledMode == LedMode::Blinking && now - lastBlink >= Config::BLINK_INTERVAL) {
        lastBlink = now;
        led.toggle();
    }

    // --- Log every N iterations ---
    ++iterCount;
    if (iterCount >= Config::LOG_EVERY_N_ITERS) {
        const uint32_t elapsed = millis() - lastLogTime;
        Serial.printf("%u iters in %u ms  (avg %.3f ms/iter)\n",
                      Config::LOG_EVERY_N_ITERS, elapsed, elapsed / (float)Config::LOG_EVERY_N_ITERS);
        iterCount   = 0;
        lastLogTime = millis();
    }
}
