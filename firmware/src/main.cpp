#include <Arduino.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <ArduinoJson.h>

// TinyGSM — must be defined before the include
#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 1024
#include <TinyGsmClient.h>

#include "config.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
HardwareSerial  modemSerial(1);
TinyGsm         modem(modemSerial);
XPowersAXP2101  PMU;

// ─── Prototypes ──────────────────────────────────────────────────────────────
bool  pmuInit();
float readBatteryVoltage();
bool  modemPowerOn();
bool  modemConnect();
float readLevelPercent();
bool  sendReading(float levelPercent, float batteryVoltage);
void  goToSleep(uint32_t seconds);

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[boot] Water tank monitor");

    // PMU must be initialised first — it controls power to the modem
    if (!pmuInit()) {
        Serial.println("[pmu] Init failed — sleeping to recover");
        goToSleep(SLEEP_INTERVAL_SEC);
    }

    float vbat = readBatteryVoltage();
    Serial.printf("[pmu] Battery: %.2fV\n", vbat);

    // Critical: PMU hard-cuts at 3.4V and board cannot self-restart after that.
    // Sleep for 2 hours and let the solar panel recover well above the cutoff.
    if (vbat < BATTERY_CRITICAL_VOLTAGE) {
        Serial.printf("[pmu] CRITICAL (%.2fV < %.2fV) — skipping all, sleeping 2h\n",
                      vbat, BATTERY_CRITICAL_VOLTAGE);
        goToSleep(SLEEP_INTERVAL_CRITICAL_SEC);
    }

    // Low: skip modem transmission to conserve power, sleep 1 hour
    if (vbat < BATTERY_LOW_VOLTAGE) {
        Serial.printf("[pmu] LOW (%.2fV < %.2fV) — skipping TX, sleeping 1h\n",
                      vbat, BATTERY_LOW_VOLTAGE);
        goToSleep(SLEEP_INTERVAL_LOW_SEC);
    }

    // Normal operation
    float level = readLevelPercent();
    Serial.printf("[sensor] Level: %.1f%%\n", level);

    if (modemPowerOn() && modemConnect()) {
        if (sendReading(level, vbat)) {
            Serial.println("[ok] Reading sent");
        } else {
            Serial.println("[warn] Send failed — will retry next cycle");
        }
        modem.gprsDisconnect();
        modem.poweroff();
    }

    goToSleep(SLEEP_INTERVAL_SEC);
}

void loop() {}  // Never reached — deep sleep restarts via setup()

// ─── PMU init ────────────────────────────────────────────────────────────────
bool pmuInit() {
    Wire.begin(PMU_SDA, PMU_SCL);

    if (!PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, PMU_SDA, PMU_SCL)) {
        Serial.println("[pmu] AXP2101 not found on I2C");
        return false;
    }

    // CRITICAL: The board has no NTC thermistor. Without disabling TS pin
    // detection the PMU will refuse to charge the battery.
    PMU.disableTSPinMeasure();

    // Set charging current — max 1A on this board
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_500MA);

    Serial.println("[pmu] AXP2101 OK, charging enabled");
    return true;
}

// ─── Battery voltage ─────────────────────────────────────────────────────────
float readBatteryVoltage() {
    // XPowersLib returns millivolts
    return PMU.getBattVoltage() / 1000.0f;
}

// ─── Modem power-on ──────────────────────────────────────────────────────────
bool modemPowerOn() {
    modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

    pinMode(MODEM_PWRKEY, OUTPUT);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(3000);

    for (int i = 0; i < 30; i++) {
        if (modem.testAT()) {
            Serial.println("[modem] AT OK");
            return true;
        }
        delay(500);
    }
    Serial.println("[modem] No response — check pins / power");
    return false;
}

// ─── Network connection ───────────────────────────────────────────────────────
bool modemConnect() {
    // LTE-M only (mode 38), CAT-M preferred (preferred mode 1)
    // 1NCE roaming tip: Balanced mode registers faster than Ultra Low Power
    modem.setNetworkMode(38);
    modem.setPreferredMode(1);

    Serial.print("[modem] Waiting for network");
    if (!modem.waitForNetwork(90000L)) {
        Serial.println("\n[modem] Network timeout");
        return false;
    }
    Serial.println(" OK");

    Serial.print("[modem] GPRS connect");
    if (!modem.gprsConnect(APN)) {
        Serial.println(" FAILED");
        return false;
    }
    Serial.println(" OK");
    return true;
}

// ─── Sensor reading ───────────────────────────────────────────────────────────
float readLevelPercent() {
    // Average multiple samples to reduce ADC noise
    long sum = 0;
    for (int i = 0; i < SENSOR_SAMPLES; i++) {
        sum += analogRead(SENSOR_PIN);
        delay(10);
    }
    float raw   = (float)sum / SENSOR_SAMPLES;
    float volts = raw * (3.3f / 4095.0f);  // 12-bit ADC, 3.3V reference

    // Map sensor voltage range to 0–100%
    float pct = (volts - SENSOR_VOLTAGE_EMPTY) /
                (SENSOR_VOLTAGE_FULL - SENSOR_VOLTAGE_EMPTY) * 100.0f;

    return constrain(pct, 0.0f, 100.0f);
}

// ─── Send to Firestore via HTTPS ──────────────────────────────────────────────
//
// POST to Firestore REST API — no Firebase SDK needed on the device.
// Document written to: /devices/{DEVICE_ID}/readings/{auto-id}
//
// Fields stored:
//   levelPercent   — tank fill level 0–100
//   batteryVoltage — device battery V (useful for remote health monitoring)
//   timestamp      — set server-side by Firestore (no device clock needed)
//
bool sendReading(float levelPercent, float batteryVoltage) {
    TinyGsmClientSecure secureClient(modem);

    const char* host = FIRESTORE_HOST;
    const int   port = 443;
    String path = "/v1/projects/" FIREBASE_PROJECT_ID
                  "/databases/(default)/documents/devices/"
                  DEVICE_ID "/readings";

    // Firestore REST document body
    StaticJsonDocument<256> doc;
    JsonObject fields = doc.createNestedObject("fields");

    JsonObject lvlField = fields.createNestedObject("levelPercent");
    lvlField["doubleValue"] = levelPercent;

    JsonObject batField = fields.createNestedObject("batteryVoltage");
    batField["doubleValue"] = batteryVoltage;

    String body;
    serializeJson(doc, body);

    Serial.printf("[http] Connecting to %s\n", host);
    if (!secureClient.connect(host, port)) {
        Serial.println("[http] Connection failed");
        return false;
    }

    secureClient.print("POST ");
    secureClient.print(path);
    secureClient.println(" HTTP/1.1");
    secureClient.print("Host: ");
    secureClient.println(host);
    secureClient.println("Content-Type: application/json");
    secureClient.print("Content-Length: ");
    secureClient.println(body.length());
    secureClient.println("Connection: close");
    secureClient.println();
    secureClient.print(body);

    String statusLine = secureClient.readStringUntil('\n');
    Serial.printf("[http] Response: %s\n", statusLine.c_str());

    secureClient.stop();
    return statusLine.indexOf("200") > 0;
}

// ─── Deep sleep ───────────────────────────────────────────────────────────────
void goToSleep(uint32_t seconds) {
    Serial.printf("[sleep] %u seconds (%.1f min)\n", seconds, seconds / 60.0f);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
}
