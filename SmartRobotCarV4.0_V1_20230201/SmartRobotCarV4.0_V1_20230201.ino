/*
 * Minimal UNO firmware for the Elegoo Robot Car V4.0.
 * Handles only serial drive commands, the onboard RGB LED, and the mode
 * button, which now requests AP/STA toggles from the ESP32-S3.
 */

#include <avr/wdt.h>
#include <ctype.h>
#include "DeviceDriverSet_xxx0.h"

namespace {

constexpr uint8_t BUTTON_PIN = 2;
constexpr uint8_t LED_ALL = 1;
constexpr unsigned long DRIVE_TIMEOUT_MS = 700;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 300;

DeviceDriverSet_Motor g_motor;
DeviceDriverSet_RBGLED g_rgbLed;

String g_serialBuffer;
unsigned long g_lastDriveCommandMs = 0;
unsigned long g_lastButtonPressMs = 0;
bool g_driveActive = false;
bool g_lastButtonLevel = HIGH;
uint8_t g_ledRed = 0;
uint8_t g_ledGreen = 0;
uint8_t g_ledBlue = 0;

bool extractIntField(const String &payload, const char *fieldName, long &value) {
  const String token = String("\"") + fieldName + "\"";
  const int fieldIndex = payload.indexOf(token);
  if (fieldIndex < 0) {
    return false;
  }

  const int colonIndex = payload.indexOf(':', fieldIndex + token.length());
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

  value = payload.substring(valueStart, valueEnd).toInt();
  return true;
}

uint8_t clampSpeed(long value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<uint8_t>(value);
}

void stopMotors() {
  g_motor.DeviceDriverSet_Motor_control(
    direction_void, 0,
    direction_void, 0,
    control_enable);
  g_driveActive = false;
}

void setLedColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (g_ledRed == red && g_ledGreen == green && g_ledBlue == blue) {
    return;
  }
  g_ledRed = red;
  g_ledGreen = green;
  g_ledBlue = blue;
  g_rgbLed.DeviceDriverSet_RBGLED_Color(LED_ALL, red, green, blue);
}

void applyDriveMode(uint8_t driveMode, uint8_t speed) {
  switch (driveMode) {
    case 1:
      g_motor.DeviceDriverSet_Motor_control(direction_just, speed, direction_just, speed, control_enable);
      g_driveActive = true;
      break;
    case 2:
      g_motor.DeviceDriverSet_Motor_control(direction_back, speed, direction_back, speed, control_enable);
      g_driveActive = true;
      break;
    case 3:
      g_motor.DeviceDriverSet_Motor_control(direction_just, speed, direction_back, speed, control_enable);
      g_driveActive = true;
      break;
    case 4:
      g_motor.DeviceDriverSet_Motor_control(direction_back, speed, direction_just, speed, control_enable);
      g_driveActive = true;
      break;
    case 5:
      g_motor.DeviceDriverSet_Motor_control(direction_just, speed, direction_just, speed / 2, control_enable);
      g_driveActive = true;
      break;
    case 6:
      g_motor.DeviceDriverSet_Motor_control(direction_back, speed, direction_back, speed / 2, control_enable);
      g_driveActive = true;
      break;
    case 7:
      g_motor.DeviceDriverSet_Motor_control(direction_just, speed / 2, direction_just, speed, control_enable);
      g_driveActive = true;
      break;
    case 8:
      g_motor.DeviceDriverSet_Motor_control(direction_back, speed / 2, direction_back, speed, control_enable);
      g_driveActive = true;
      break;
    default:
      stopMotors();
      break;
  }

  if (g_driveActive) {
    g_lastDriveCommandMs = millis();
  }
}

void handleLightingCommand(const String &payload) {
  long red = 0;
  long green = 0;
  long blue = 0;
  extractIntField(payload, "D2", red);
  extractIntField(payload, "D3", green);
  extractIntField(payload, "D4", blue);
  setLedColor(clampSpeed(red), clampSpeed(green), clampSpeed(blue));
}

void handleCommand(const String &payload) {
  if (payload.length() == 0) {
    return;
  }

  if (payload == "{Factory}") {
    Serial.println("{UNO_READY}");
    return;
  }

  long command = -1;
  if (!extractIntField(payload, "N", command)) {
    return;
  }

  switch (command) {
    case 1:
    case 100:
    case 110:
      stopMotors();
      break;
    case 7:
    case 8:
      handleLightingCommand(payload);
      break;
    case 102: {
      long driveMode = 9;
      long speed = 0;
      extractIntField(payload, "D1", driveMode);
      extractIntField(payload, "D2", speed);
      applyDriveMode(static_cast<uint8_t>(driveMode), clampSpeed(speed));
      break;
    }
    default:
      break;
  }
}

void readSerialCommands() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());

    if (c == '\r' || c == '\n') {
      if (g_serialBuffer.length() > 0) {
        handleCommand(g_serialBuffer);
        g_serialBuffer = "";
      }
      continue;
    }

    g_serialBuffer += c;
    if (c == '}') {
      handleCommand(g_serialBuffer);
      g_serialBuffer = "";
    }

    if (g_serialBuffer.length() > 160) {
      g_serialBuffer = "";
    }
  }
}

void enforceDriveTimeout() {
  if (g_driveActive && millis() - g_lastDriveCommandMs > DRIVE_TIMEOUT_MS) {
    stopMotors();
  }
}

void pollWifiToggleButton() {
  const bool buttonLevel = digitalRead(BUTTON_PIN);
  if (g_lastButtonLevel == HIGH && buttonLevel == LOW && millis() - g_lastButtonPressMs > BUTTON_DEBOUNCE_MS) {
    g_lastButtonPressMs = millis();
    Serial.println("{WIFI_TOGGLE}");
    setLedColor(255, 100, 0);
  }

  if (buttonLevel == HIGH && !g_driveActive) {
    setLedColor(0, 24, 0);
  }

  g_lastButtonLevel = buttonLevel;
}

}  // namespace

void setup() {
  Serial.begin(9600);
  g_motor.DeviceDriverSet_Motor_Init();
  g_rgbLed.DeviceDriverSet_RBGLED_Init(20);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  stopMotors();
  setLedColor(0, 24, 0);
  wdt_enable(WDTO_2S);
  Serial.println("{UNO_READY}");
}

void loop() {
  wdt_reset();
  readSerialCommands();
  enforceDriveTimeout();
  pollWifiToggleButton();
}
