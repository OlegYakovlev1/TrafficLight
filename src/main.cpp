#include <Arduino.h>
#include <atomic>
#include <cstdint>

constexpr uint8_t LED_RED_PIN = 16;
constexpr uint8_t LED_YELLOW_PIN = 17;
constexpr uint8_t LED_GREEN_PIN = 18;
constexpr uint8_t BUTTON_PIN = 15;
constexpr uint8_t LDR_PIN = 1;

constexpr uint8_t RED_CHANNEL = 0;
constexpr uint8_t YELLOW_CHANNEL = 1;
constexpr uint8_t GREEN_CHANNEL = 2;

constexpr uint32_t PWM_FREQ = 5000;
constexpr uint8_t PWM_RESOLUTION = 8;

constexpr uint32_t DEBOUNCE_DELAY_MS = 50;

constexpr uint32_t RED_DURATION_MS = 3000;
constexpr uint32_t RED_YELLOW_DURATION_MS = 1000;
constexpr uint32_t GREEN_DURATION_MS = 3000;
constexpr uint32_t YELLOW_DURATION_MS = 1000;
constexpr uint32_t SERVICE_BLINK_INTERVAL_MS = 500;

constexpr uint16_t ADC_MIN = 0;
constexpr uint16_t ADC_MAX = 4095;
constexpr uint8_t BRIGHTNESS_MIN = 40;
constexpr uint8_t BRIGHTNESS_MAX = 255;

enum class Mode : uint8_t {
  Normal,
  Service
};

enum class NormalModeState : uint8_t {
  Red,
  RedYellow,
  Green,
  Yellow
};

Mode currentMode = Mode::Normal;
NormalModeState currentNormalState = NormalModeState::Red;

std::atomic<bool> buttonInterruptFlag{false};

bool waitingForRelease = false;
uint32_t lastPressTime = 0;

uint32_t stateStartTime = 0;
uint32_t lastBlinkTime = 0;

bool serviceYellowOn = false;
uint8_t brightness = 200;

void IRAM_ATTR handleButtonInterrupt() {
  buttonInterruptFlag = true;
}

const char* getModeName(const Mode mode) {
  switch (mode) {
    case Mode::Normal: return "NORMAL";
    case Mode::Service: return "SERVICE";
    default: return "UNKNOWN";
  }
}

const char* getStateName(const NormalModeState state) {
  switch (state) {
    case NormalModeState::Red: return "RED";
    case NormalModeState::RedYellow: return "RED_YELLOW";
    case NormalModeState::Green: return "GREEN";
    case NormalModeState::Yellow: return "YELLOW";
    default: return "UNKNOWN";
  }
}

void setLedBrightness(uint8_t r, uint8_t y, uint8_t g) {
  ledcWrite(RED_CHANNEL, r);
  ledcWrite(YELLOW_CHANNEL, y);
  ledcWrite(GREEN_CHANNEL, g);
}

void setNormalState(NormalModeState state) {
  currentNormalState = state;
  stateStartTime = millis();

  Serial.print("State: ");
  Serial.println(getStateName(state));
}

void switchMode() {
  switch (currentMode) {
    case Mode::Normal:
      currentMode = Mode::Service;
      serviceYellowOn = false;
      lastBlinkTime = millis();
      Serial.println("→ SERVICE");
      break;

    case Mode::Service:
      currentMode = Mode::Normal;
      setNormalState(NormalModeState::Red);
      Serial.println("→ NORMAL");
      break;

    default:
      currentMode = Mode::Normal;
      setNormalState(NormalModeState::Red);
      break;
  }
}

void handleButton() {
  if (buttonInterruptFlag.exchange(false)) {
    const uint32_t now = millis();

    if (now - lastPressTime >= DEBOUNCE_DELAY_MS) {
      if (digitalRead(BUTTON_PIN) == LOW && !waitingForRelease) {
        switchMode();
        lastPressTime = now;
        waitingForRelease = true;
      }
    }
  }

  if (waitingForRelease && digitalRead(BUTTON_PIN) == HIGH) {
    waitingForRelease = false;
  }
}

void updateBrightness() {
  uint16_t ldr = analogRead(LDR_PIN);

  brightness = constrain(
    map(ldr, ADC_MIN, ADC_MAX, BRIGHTNESS_MIN, BRIGHTNESS_MAX),
    BRIGHTNESS_MIN,
    BRIGHTNESS_MAX
  );

  static uint32_t lastLogTime = 0;
  if (millis() - lastLogTime > 1000) {
    Serial.print("Mode: ");
    Serial.print(getModeName(currentMode));
    Serial.print(" | LDR: ");
    Serial.print(ldr);
    Serial.print(" | Bright: ");
    Serial.println(brightness);
    lastLogTime = millis();
  }
}

void updateNormalMode() {
  uint32_t now = millis();
  uint32_t dt = now - stateStartTime;

  switch (currentNormalState) {
    case NormalModeState::Red:
      if (dt >= RED_DURATION_MS)
        setNormalState(NormalModeState::RedYellow);
      break;

    case NormalModeState::RedYellow:
      if (dt >= RED_YELLOW_DURATION_MS)
        setNormalState(NormalModeState::Green);
      break;

    case NormalModeState::Green:
      if (dt >= GREEN_DURATION_MS)
        setNormalState(NormalModeState::Yellow);
      break;

    case NormalModeState::Yellow:
      if (dt >= YELLOW_DURATION_MS)
        setNormalState(NormalModeState::Red);
      break;

    default:
      setNormalState(NormalModeState::Red);
      break;
  }
}

void updateServiceMode() {
  uint32_t now = millis();

  if (now - lastBlinkTime >= SERVICE_BLINK_INTERVAL_MS) {
    serviceYellowOn = !serviceYellowOn;
    lastBlinkTime = now;
  }
}

void getNormalLeds(uint8_t& r, uint8_t& y, uint8_t& g) {
  switch (currentNormalState) {
    case NormalModeState::Red:       r = brightness; break;
    case NormalModeState::RedYellow: r = brightness; y = brightness; break;
    case NormalModeState::Green:     g = brightness; break;
    case NormalModeState::Yellow:    y = brightness; break;
    default:                         r = brightness; break;
  }
}

void renderLeds() {
  uint8_t r = 0, y = 0, g = 0;

  switch (currentMode) {
    case Mode::Normal:
      getNormalLeds(r, y, g);
      break;

    case Mode::Service:
      y = serviceYellowOn ? brightness : 0;
      break;

    default:
      r = brightness;
      break;
  }

  setLedBrightness(r, y, g);
}

void setup() {
  Serial.begin(9600);
  delay(300);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  analogReadResolution(12);

  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(YELLOW_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  ledcAttachPin(LED_RED_PIN, RED_CHANNEL);
  ledcAttachPin(LED_YELLOW_PIN, YELLOW_CHANNEL);
  ledcAttachPin(LED_GREEN_PIN, GREEN_CHANNEL);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  stateStartTime = millis();
  lastBlinkTime = millis();

  Serial.println("Traffic light started");
  setNormalState(NormalModeState::Red);
}

void loop() {
  handleButton();
  updateBrightness();

  switch (currentMode) {
    case Mode::Normal:
      updateNormalMode();
      break;

    case Mode::Service:
      updateServiceMode();
      break;

    default:
      currentMode = Mode::Normal;
      setNormalState(NormalModeState::Red);
      break;
  }

  renderLeds();
}