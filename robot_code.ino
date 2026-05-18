#include <BluetoothSerial.h>
#include <Wire.h>
#include <ESP32Servo.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "Adafruit_TCS34725.h"

BluetoothSerial SerialBT;

// =====================================================
// ================= OLED DISPLAY ======================
// =====================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

// =====================================================
// ================= MOTOR PINS ========================
// =====================================================

#define IN1 25
#define IN2 26
#define IN3 32
#define IN4 33

// =====================================================
// ================= ENABLE PINS =======================
// =====================================================

#define ENA 17
#define ENB 14

// =====================================================
// ================= LINE SENSORS ======================
// =====================================================

#define LEFT_SENSOR 34
#define RIGHT_SENSOR 35

// =====================================================
// ================= SERVOS ============================
// =====================================================

#define LEFT_SERVO_PIN 12
#define RIGHT_SERVO_PIN 13

Servo leftServo;
Servo rightServo;

// START CLOSED
float leftServoPos = 135;
float rightServoPos = 45;

// =====================================================
// ================= COLOR SENSOR ======================
// =====================================================

Adafruit_TCS34725 tcs =
Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_50MS,
  TCS34725_GAIN_4X
);

// =====================================================
// ================= VARIABLES =========================
// =====================================================

bool autoMode = false;

int lastDirection = 1;

String currentColor = "UNKNOWN";

String movementState = "STOP";

// =====================================================
// ================= FUNCTION DECLARATIONS =============
// =====================================================

void updateDisplay();

void forward();
void backward();
void left();
void right();

void softLeft();
void softRight();

void searchLeft();
void searchRight();

void stopMotors();

String detectColor();

void openClaw();
void closeClaw();

// =====================================================
// ================= SETUP =============================
// =====================================================

void setup() {

  Serial.begin(115200);

  // =====================================================
  // ================= I2C ===============================
  // =====================================================

  Wire.begin(21, 22);

  // =====================================================
  // ================= OLED ==============================
  // =====================================================

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {

    Serial.println("OLED FAILED");

    while (1);
  }

  display.clearDisplay();
  display.display();

  // =====================================================
  // ================= BLUETOOTH =========================
  // =====================================================

  SerialBT.begin("ESP32_CAR");

  // =====================================================
  // ================= MOTOR PINS ========================
  // =====================================================

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // =====================================================
  // ================= ENABLE PINS =======================
  // =====================================================

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  // =====================================================
  // ================= LINE SENSORS ======================
  // =====================================================

  pinMode(LEFT_SENSOR, INPUT);
  pinMode(RIGHT_SENSOR, INPUT);

  // =====================================================
  // ================= SERVOS ============================
  // =====================================================

  leftServo.attach(LEFT_SERVO_PIN);
  rightServo.attach(RIGHT_SERVO_PIN);

  leftServo.write(leftServoPos);
  rightServo.write(rightServoPos);

  // =====================================================
  // ================= COLOR SENSOR ======================
  // =====================================================

  if (tcs.begin()) {

    Serial.println("TCS34725 FOUND");
  }
  else {

    Serial.println("NO TCS34725 FOUND");
  }

  stopMotors();

  updateDisplay();

  Serial.println("READY");
}

// =====================================================
// ================= LOOP ==============================
// =====================================================

void loop() {

  // =====================================================
  // ================= COLOR DETECTION ===================
  // =====================================================

  currentColor = detectColor();

  // =====================================================
  // ================= BLUETOOTH =========================
  // =====================================================

  if (SerialBT.available()) {

    char command = SerialBT.read();

    Serial.print("CMD: ");
    Serial.println(command);

    // =================================================
    // AUTO MODE
    // =================================================

    if (command == 'N' || command == 'n') {

      autoMode = !autoMode;

      stopMotors();

      delay(300);
    }

    // =================================================
    // CLAW CONTROL
    // =================================================

    // M = OPEN
    if (command == 'M') {

      openClaw();
    }

    // m = CLOSE
    if (command == 'm') {

      closeClaw();
    }

    // =================================================
    // MANUAL CONTROL
    // =================================================

    if (!autoMode) {

      switch(command) {

        case 'F':
          forward();
          break;

        case 'B':
          backward();
          break;

        case 'L':
          left();
          break;

        case 'R':
          right();
          break;

        case 'S':
          stopMotors();
          break;
      }
    }
  }

  // =====================================================
  // ================= AUTO LINE FOLLOW ==================
  // =====================================================

  if (autoMode) {

    int leftSensor = digitalRead(LEFT_SENSOR);
    int rightSensor = digitalRead(RIGHT_SENSOR);

    // BOTH WHITE
    if (leftSensor == 1 && rightSensor == 1) {

      forward();
    }

    // LEFT SENSOR ON BLACK
    else if (leftSensor == 0 && rightSensor == 1) {

      lastDirection = -1;

      softLeft();
    }

    // RIGHT SENSOR ON BLACK
    else if (leftSensor == 1 && rightSensor == 0) {

      lastDirection = 1;

      softRight();
    }

    // BOTH BLACK
    else {

      if (lastDirection == -1) {

        searchLeft();
      }
      else {

        searchRight();
      }
    }
  }

  updateDisplay();
}

// =====================================================
// ================= OLED DISPLAY ======================
// =====================================================

void updateDisplay() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  // TITLE
  display.setCursor(0, 0);
  display.println("ESP32 ROBOT");

  // MODE
  display.setCursor(0, 12);

  if (autoMode) {

    display.println("MODE: AUTO");
  }
  else {

    display.println("MODE: MANUAL");
  }

  // MOVEMENT
  display.setCursor(0, 24);
  display.print("MOVE: ");
  display.println(movementState);

  // COLOR
  display.setCursor(0, 36);
  display.print("COLOR: ");
  display.println(currentColor);

  // CLAW
  display.setCursor(0, 48);
  display.print("CLAW: ");
  display.print(leftServoPos);
  display.print("/");
  display.println(rightServoPos);

  display.display();
}

// =====================================================
// ================= COLOR FUNCTION ====================
// =====================================================

String detectColor() {

  uint16_t r, g, b, c;

  tcs.getRawData(&r, &g, &b, &c);

  if (c < 50) {

    return "UNKNOWN";
  }

  if (r > g && r > b) {

    return "RED";
  }

  else if (g > r && g > b) {

    return "GREEN";
  }

  else if (b > r && b > g) {

    return "BLUE";
  }

  else {

    return "UNKNOWN";
  }
}

// =====================================================
// ================= CLAW FUNCTIONS ====================
// =====================================================

// OPEN BY 45°
void openClaw() {

  leftServoPos -= 45;
  rightServoPos += 45;

  leftServoPos = constrain(leftServoPos, 0, 180);
  rightServoPos = constrain(rightServoPos, 0, 180);

  leftServo.write(leftServoPos);
  rightServo.write(rightServoPos);
}

// CLOSE BY 45°
void closeClaw() {

  leftServoPos += 45;
  rightServoPos -= 45;

  leftServoPos = constrain(leftServoPos, 0, 180);
  rightServoPos = constrain(rightServoPos, 0, 180);

  leftServo.write(leftServoPos);
  rightServo.write(rightServoPos);
}

// =====================================================
// ================= MOTOR FUNCTIONS ===================
// =====================================================

void forward() {

  movementState = "FORWARD";

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {

  movementState = "BACKWARD";

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void left() {

  movementState = "LEFT";

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void right() {

  movementState = "RIGHT";

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void softLeft() {

  movementState = "SOFT LEFT";

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void softRight() {

  movementState = "SOFT RIGHT";

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void searchLeft() {

  movementState = "SEARCH LEFT";

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void searchRight() {

  movementState = "SEARCH RIGHT";

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {

  movementState = "STOP";

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}