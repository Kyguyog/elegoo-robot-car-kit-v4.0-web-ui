/*
 * Minimal UNO bridge for ESP32S3 control.
 * Supports motors, RGB LED, mode button, and battery voltage reporting.
 */

#include <FastLED.h>

#define PIN_RBGLED 4
#define NUM_LEDS 1
#define MOTOR_PWMA 5
#define MOTOR_PWMB 6
#define MOTOR_AIN1 7
#define MOTOR_BIN1 8
#define MOTOR_STBY 3
#define PIN_BUTTON 2
#define PIN_BATTERY A3
#define SERIAL_BAUD 9600

CRGB leds[NUM_LEDS];

static const uint8_t CMD_MOTOR = 102;
static const unsigned long BUTTON_DEBOUNCE_MS = 60;
static unsigned long lastStatusMillis = 0;
static bool lastButtonState = false;
static float lastBatteryVoltage = 0.0f;
static int rawButtonState = HIGH;
static int debouncedButtonState = HIGH;
static unsigned long buttonChangeMillis = 0;
static String lineBuffer;
static uint8_t lastSpeed = 180;
static uint8_t currentR = 0;
static uint8_t currentG = 0;
static uint8_t currentB = 0;
static bool blinkActive = false;
static unsigned long blinkStart = 0;
static const unsigned long BLINK_DURATION_MS = 120;

void stopMotor() {
  digitalWrite(MOTOR_STBY, LOW);
  analogWrite(MOTOR_PWMA, 0);
  analogWrite(MOTOR_PWMB, 0);
}

void motorDrive(uint8_t directionCode, uint8_t speed) {
  uint8_t reducedSpeed = speed / 2;
  if (reducedSpeed == 0) {
    reducedSpeed = 1;
  }

  digitalWrite(MOTOR_STBY, HIGH);

  switch (directionCode) {
    case 1: // forward
      digitalWrite(MOTOR_AIN1, HIGH);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, HIGH);
      analogWrite(MOTOR_PWMB, speed);
      break;
    case 2: // backward
      digitalWrite(MOTOR_AIN1, LOW);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, LOW);
      analogWrite(MOTOR_PWMB, speed);
      break;
    case 3: // turn left in place
      digitalWrite(MOTOR_AIN1, LOW);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, HIGH);
      analogWrite(MOTOR_PWMB, speed);
      break;
    case 4: // turn right in place
      digitalWrite(MOTOR_AIN1, HIGH);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, LOW);
      analogWrite(MOTOR_PWMB, speed);
      break;
    case 5: // left-forward
      digitalWrite(MOTOR_AIN1, HIGH);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, HIGH);
      analogWrite(MOTOR_PWMB, reducedSpeed);
      break;
    case 6: // left-backward
      digitalWrite(MOTOR_AIN1, LOW);
      analogWrite(MOTOR_PWMA, speed);
      digitalWrite(MOTOR_BIN1, LOW);
      analogWrite(MOTOR_PWMB, reducedSpeed);
      break;
    case 7: // right-forward
      digitalWrite(MOTOR_AIN1, HIGH);
      analogWrite(MOTOR_PWMA, reducedSpeed);
      digitalWrite(MOTOR_BIN1, HIGH);
      analogWrite(MOTOR_PWMB, speed);
      break;
    case 8: // right-backward
      digitalWrite(MOTOR_AIN1, LOW);
      analogWrite(MOTOR_PWMA, reducedSpeed);
      digitalWrite(MOTOR_BIN1, LOW);
      analogWrite(MOTOR_PWMB, speed);
      break;
    default:
      stopMotor();
      break;
  }
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  currentR = r;
  currentG = g;
  currentB = b;
  leds[0] = CRGB(r, g, b);
  FastLED.show();
}

void blinkLedOnce() {
  if (blinkActive) return;
  blinkActive = true;
  blinkStart = millis();
  leds[0] = CRGB(255, 255, 255);
  FastLED.show();
}

void updateBlink() {
  if (!blinkActive) return;
  if (millis() - blinkStart >= BLINK_DURATION_MS) {
    blinkActive = false;
    leds[0] = CRGB(currentR, currentG, currentB);
    FastLED.show();
  }
}

float readBatteryVoltage() {
  return analogRead(PIN_BATTERY) * 0.0375f;
}

bool readButtonPressedRaw() {
  return digitalRead(PIN_BUTTON) == LOW;
}

bool readButtonPressed() {
  int currentRaw = digitalRead(PIN_BUTTON);
  if (currentRaw != rawButtonState) {
    rawButtonState = currentRaw;
    buttonChangeMillis = millis();
  }

  if (currentRaw != debouncedButtonState && millis() - buttonChangeMillis >= BUTTON_DEBOUNCE_MS) {
    debouncedButtonState = currentRaw;
  }

  return debouncedButtonState == LOW;
}

bool parseJsonInt(const String &text, const char *key, int &value) {
  int idx = text.indexOf(key);
  if (idx < 0) return false;
  idx += strlen(key);
  idx = text.indexOf(':', idx);
  if (idx < 0) return false;
  idx++;
  while (idx < text.length() && (text[idx] == ' ' || text[idx] == '\t')) idx++;
  int start = idx;
  if (idx < text.length() && (text[idx] == '-' || text[idx] == '+')) idx++;
  while (idx < text.length() && (text[idx] >= '0' && text[idx] <= '9')) idx++;
  if (idx == start) return false;
  value = text.substring(start, idx).toInt();
  return true;
}

void sendStatus() {
  bool buttonPressed = readButtonPressed();
  float voltage = readBatteryVoltage();
  float diff = voltage - lastBatteryVoltage;
  if (buttonPressed == lastButtonState && diff > -0.05f && diff < 0.05f) {
    return;
  }
  lastButtonState = buttonPressed;
  lastBatteryVoltage = voltage;

  Serial.print("{\"BTN\":");
  Serial.print(buttonPressed ? 1 : 0);
  Serial.print(",\"V\":");
  Serial.print(voltage, 2);
  Serial.println("}");
}

void processCommand(const String &cmd) {
  String text = cmd;
  text.trim();
  if (text.length() == 0) return;

  if (text.equalsIgnoreCase("w") || text.equalsIgnoreCase("a") || text.equalsIgnoreCase("s") || text.equalsIgnoreCase("d") || text.equalsIgnoreCase("x")) {
    lastSpeed = constrain(lastSpeed, 0, 255);
    if (text.equalsIgnoreCase("w")) {
      motorDrive(1, lastSpeed);
    } else if (text.equalsIgnoreCase("s")) {
      motorDrive(2, lastSpeed);
    } else if (text.equalsIgnoreCase("a")) {
      motorDrive(3, lastSpeed);
    } else if (text.equalsIgnoreCase("d")) {
      motorDrive(4, lastSpeed);
    } else {
      stopMotor();
    }
    return;
  }

  int command = 0;
  int direction = 0;
  int speed = -1;
  int red = -1;
  int green = -1;
  int blue = -1;

  if (parseJsonInt(text, "N", command)) {
    if (command == 100 || command == 1) {
      stopMotor();
      return;
    }
  }
  parseJsonInt(text, "D1", direction);
  parseJsonInt(text, "D2", speed);
  parseJsonInt(text, "R", red);
  parseJsonInt(text, "G", green);
  parseJsonInt(text, "B", blue);

  if (command == CMD_MOTOR && direction > 0 && speed >= 0) {
    lastSpeed = constrain(speed, 0, 255);
    motorDrive(direction, lastSpeed);
  }

  if (red >= 0 || green >= 0 || blue >= 0) {
    setRgb(red < 0 ? 0 : constrain(red, 0, 255),
           green < 0 ? 0 : constrain(green, 0, 255),
           blue < 0 ? 0 : constrain(blue, 0, 255));
  }

  int blink = 0;
  if (parseJsonInt(text, "L", blink) && blink > 0) {
    blinkLedOnce();
  }
}

void setup() {
  pinMode(MOTOR_PWMA, OUTPUT);
  pinMode(MOTOR_PWMB, OUTPUT);
  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_BIN1, OUTPUT);
  pinMode(MOTOR_STBY, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BATTERY, INPUT);

  FastLED.addLeds<NEOPIXEL, PIN_RBGLED>(leds, NUM_LEDS);
  setRgb(0, 0, 0);

  Serial.begin(SERIAL_BAUD);
  stopMotor();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') {
      continue;
    }
    lineBuffer += c;
    if (c == '}' || c == '\n') {
      processCommand(lineBuffer);
      lineBuffer = "";
    }
  }

  if (millis() - lastStatusMillis > 150) {
    lastStatusMillis = millis();
    sendStatus();
  }

  updateBlink();
}
