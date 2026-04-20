# Traffic Light — ESP32-S3

A simple traffic light controller built on the **Freenove ESP32-S3 WROOM** using the Arduino framework via PlatformIO. The device cycles through standard traffic-light phases, supports a service (blinking) mode, and automatically adjusts LED brightness based on ambient light.

## Features

- **Normal mode** — standard 4-phase cycle: Red → Red+Yellow → Green → Yellow → Red
- **Service mode** — yellow LED blinks at 500 ms intervals (e.g., intersection out-of-service)
- **Button-driven mode switching** with hardware interrupt and software debounce
- **Adaptive brightness** — an LDR (photoresistor) controls LED intensity via PWM, so the lights are dim at night and bright during the day
- **Non-blocking loop** — all timing is done with `millis()`; no `delay()` in the main state machine
- **Serial logging** — current mode, state, LDR reading, and brightness

## Hardware

| Component  | GPIO | Notes                               |
| ---------- | ---- | ----------------------------------- |
| Red LED    | 16   | PWM channel 0                       |
| Yellow LED | 17   | PWM channel 1                       |
| Green LED  | 18   | PWM channel 2                       |
| Button     | 15   | `INPUT_PULLUP`, triggers on FALLING |
| LDR        | 1    | Analog input, 12-bit ADC            |

Each LED should be wired through a current-limiting resistor (220Ω).
The LDR is used in a voltage-divider configuration with a 10 kΩ resistor to GND.

### PWM configuration

- Frequency: **5 kHz**
- Resolution: **8 bits** (0–255)

### Brightness range

ADC values (0–4095) are mapped to a brightness range of **40–255**, so the LEDs never go fully dark.

## State machine

### Normal mode timings

| Phase        | Duration |
| ------------ | -------- |
| Red          | 3000 ms  |
| Red + Yellow | 1000 ms  |
| Green        | 3000 ms  |
| Yellow       | 1000 ms  |

### Service mode

Only the yellow LED toggles on/off every 500 ms. Red and green stay off.

### Mode switching

Pressing the button toggles between **Normal** and **Service** modes. The interrupt sets a flag, which is processed in the main loop with a 50 ms debounce and release detection to prevent repeated triggers from a single press.

## Notes

- Written in modern C++: `enum class`, `constexpr`, `std::atomic` for interrupt-safe flag handling.
- `IRAM_ATTR` is used on the ISR so it executes from IRAM instead of flash, which is required for ESP32 interrupts.
- All state transitions and I/O happen non-blockingly in `loop()`, keeping the system responsive to button presses.
