#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// LILYGO T-SIM7080G-S3 pin map
// Cross-check against: https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G
// ─────────────────────────────────────────────────────────────────────────────
#define MODEM_TX_PIN    17
#define MODEM_RX_PIN    18
#define MODEM_PWRKEY    41
#define MODEM_RESET     42
#define MODEM_BAUD      115200

// AXP2101 PMU (power management) — I2C
// Pins confirmed from LILYGO GitHub README
#define PMU_SDA         15
#define PMU_SCL         7
#define PMU_IRQ         6

// ─────────────────────────────────────────────────────────────────────────────
// Sensor
// ─────────────────────────────────────────────────────────────────────────────

// Analog pin for 4-20mA pressure transducer (via 165Ω resistor to ADC)
// This gives 0.66V (empty) to 3.3V (full) — safely within ESP32-S3 ADC range
// Alternatively use a 0.5-4.5V transducer through a 2:1 voltage divider
#define SENSOR_PIN          1       // GPIO1 (ADC1_CH0) — confirm ADC-capable on your board

// Calibration: adjust these to match your specific sensor and tank
#define SENSOR_VOLTAGE_EMPTY    0.66f   // Volts at empty tank (4mA * 165Ω)
#define SENSOR_VOLTAGE_FULL     3.30f   // Volts at full tank (20mA * 165Ω)
#define TANK_HEIGHT_CM          200     // Physical tank height in centimetres

// Number of ADC samples to average (reduces noise)
#define SENSOR_SAMPLES          8

// ─────────────────────────────────────────────────────────────────────────────
// Connectivity — 1NCE SIM
// ─────────────────────────────────────────────────────────────────────────────
#define APN             "iot.1nce.net"
// 1NCE needs no username/password
// Use Balanced (not Ultra Low Power) registration for Australian roaming

// ─────────────────────────────────────────────────────────────────────────────
// Firebase Firestore REST
// ─────────────────────────────────────────────────────────────────────────────
// Replace with your actual Firebase project ID (see Firebase console → Project settings)
#define FIREBASE_PROJECT_ID     "your-firebase-project-id"

// Unique ID for this physical device — use something meaningful, e.g. "house-tank"
// Must match a document under /devices/ in Firestore
#define DEVICE_ID               "tank-01"

// Firestore REST endpoint (HTTPS)
// POST to: https://firestore.googleapis.com/v1/projects/{PROJECT_ID}/databases/(default)/documents/devices/{DEVICE_ID}/readings
#define FIRESTORE_HOST          "firestore.googleapis.com"

// ─────────────────────────────────────────────────────────────────────────────
// Battery protection
// ─────────────────────────────────────────────────────────────────────────────
// The AXP2101 PMU hard-cuts power at 3.4V. If that happens remotely the board
// cannot self-restart even after solar recharges — a manual button press is
// required. These thresholds keep us safely above that point.
#define BATTERY_LOW_VOLTAGE       3.55f   // Skip transmission, sleep 1 h
#define BATTERY_CRITICAL_VOLTAGE  3.45f   // Don't wake modem at all, sleep 2 h

// ─────────────────────────────────────────────────────────────────────────────
// Timing
// ─────────────────────────────────────────────────────────────────────────────
// Wake interval in seconds. 15 minutes = 96 readings/day.
// 1NCE 500MB lifetime — each HTTPS POST is ~1KB → ~500,000 readings total.
// At 96/day that's ~14 years of data on one SIM purchase.
#define SLEEP_INTERVAL_SEC           (15 * 60)   // Normal
#define SLEEP_INTERVAL_LOW_SEC       (60 * 60)   // Low battery  — 1 hour
#define SLEEP_INTERVAL_CRITICAL_SEC  (2 * 60 * 60) // Critical battery — 2 hours
