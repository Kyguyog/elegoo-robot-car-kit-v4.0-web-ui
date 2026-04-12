/*
 * ESP32-S3 control bridge for the Elegoo robot car.
 * Hosts the web UI, forwards drive commands to the UNO, and enforces a
 * failsafe so the car stops if command refreshes stop arriving.
 */

#include "CameraWebServer_AP.h"
#include <WiFi.h>
#include <ctype.h>
#include "esp_system.h"

#define RXD2 3
#define TXD2 40
#define STATUS_LED_PIN 46

namespace {

constexpr unsigned long DRIVE_COMMAND_TIMEOUT_MS = 700;
constexpr char UNO_WIFI_TOGGLE_MESSAGE[] = "{WIFI_TOGGLE}";
constexpr char UNO_STOP_COMMAND[] = "{\"N\":100}\n";

CameraWebServer_AP g_cameraWebServerAp;
String g_serial2Buffer;
unsigned long g_lastDriveCommandMs = 0;
bool g_driveCommandActive = false;
bool g_restartRequested = false;
unsigned long g_restartAtMs = 0;

bool payloadContainsIntField(const String &payload, const char *fieldName, int expectedValue) {
  const String quotedField = String("\"") + fieldName + "\"";
  int fieldIndex = payload.indexOf(quotedField);
  if (fieldIndex < 0) {
    return false;
  }

  int colonIndex = payload.indexOf(':', fieldIndex + quotedField.length());
  if (colonIndex < 0) {
    return false;
  }

  int valueStart = colonIndex + 1;
  while (valueStart < payload.length() && isspace(static_cast<unsigned char>(payload[valueStart]))) {
    valueStart++;
  }

  int valueEnd = valueStart;
  if (valueEnd < payload.length() && (payload[valueEnd] == '-' || payload[valueEnd] == '+')) {
    valueEnd++;
  }
  while (valueEnd < payload.length() && isdigit(static_cast<unsigned char>(payload[valueEnd]))) {
    valueEnd++;
  }

  if (valueEnd == valueStart) {
    return false;
  }

  return payload.substring(valueStart, valueEnd).toInt() == expectedValue;
}

bool isDriveStartPayload(const String &payload) {
  return payloadContainsIntField(payload, "N", 102);
}

bool isDriveStopPayload(const String &payload) {
  return payloadContainsIntField(payload, "N", 100) ||
         payloadContainsIntField(payload, "N", 110) ||
         payloadContainsIntField(payload, "N", 1);
}

void forwardStopToUno(const char *reason) {
  Serial.printf("Forwarding stop to UNO (%s)\n", reason);
  Serial2.print(UNO_STOP_COMMAND);
}

void scheduleRestart(unsigned long delayMs) {
  g_restartRequested = true;
  g_restartAtMs = millis() + delayMs;
}

void handleUnoMessage(const String &message) {
  if (message.length() == 0) {
    return;
  }

  Serial.print("UNO -> ESP32: ");
  Serial.println(message);

  if (message == UNO_WIFI_TOGGLE_MESSAGE) {
    String statusMessage;
    if (toggleConfiguredNetworkMode(statusMessage)) {
      Serial.println(statusMessage);
      forwardStopToUno("network mode toggle");
      g_driveCommandActive = false;
      scheduleRestart(300);
    } else {
      Serial.print("Wi-Fi mode toggle failed: ");
      Serial.println(statusMessage);
    }
  }
}

void processUnoSerial() {
  while (Serial2.available()) {
    const char c = static_cast<char>(Serial2.read());

    if (c == '\r' || c == '\n') {
      if (g_serial2Buffer.length() > 0) {
        handleUnoMessage(g_serial2Buffer);
        g_serial2Buffer = "";
      }
      continue;
    }

    g_serial2Buffer += c;
    if (c == '}') {
      handleUnoMessage(g_serial2Buffer);
      g_serial2Buffer = "";
    }

    if (g_serial2Buffer.length() > 160) {
      g_serial2Buffer = "";
    }
  }
}

void enforceDriveFailsafe() {
  if (!g_driveCommandActive) {
    return;
  }

  if (millis() - g_lastDriveCommandMs > DRIVE_COMMAND_TIMEOUT_MS) {
    g_driveCommandActive = false;
    forwardStopToUno("drive lease expired");
  }
}

void servicePendingRestart() {
  if (g_restartRequested && static_cast<long>(millis() - g_restartAtMs) >= 0) {
    Serial.println("Rebooting to apply Wi-Fi mode change");
    delay(50);
    ESP.restart();
  }
}

}  // namespace

void notifyDriveCommandForwarded(const String &payload) {
  if (isDriveStartPayload(payload)) {
    g_driveCommandActive = true;
    g_lastDriveCommandMs = millis();
    return;
  }

  if (isDriveStopPayload(payload)) {
    g_driveCommandActive = false;
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);

  Serial.print("network_id:");
  g_cameraWebServerAp.CameraWebServer_AP_Init();
  forwardStopToUno("startup");
  Serial.println("Control bridge ready");
}

void loop() {
  processUnoSerial();
  enforceDriveFailsafe();
  servicePendingRestart();
}
