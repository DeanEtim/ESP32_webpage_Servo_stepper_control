#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "htmlWebpage.h"

// Pin Connections
#define SERVO_PIN 19
#define IN1 12
#define IN2 27
#define IN3 14
#define IN4 25

Servo myServo;

// Stepper sequence (half-step mode for smooth rotation)
const int stepSequence[8][4] = {
  { 1, 0, 0, 0 },
  { 1, 1, 0, 0 },
  { 0, 1, 0, 0 },
  { 0, 1, 1, 0 },
  { 0, 0, 1, 0 },
  { 0, 0, 1, 1 },
  { 0, 0, 0, 1 },
  { 1, 0, 0, 1 }
};

int stepIndex = 0;
int stepDelay = 10;  // default delay (speed)
bool stepperCW = true;

// Servo control variables
int currentServoAngle = 90;
int targetServoAngle = 90;
const int SERVO_STEP_DELAY = 10;  // fixed speed

// WiFi and WebSocket Config
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Wifi credentials
const char *ssid = "ESP32_Control_Dashboard";
const char *password = "12345678";

void stepperStep() {
  digitalWrite(IN1, stepSequence[stepIndex][0]);
  digitalWrite(IN2, stepSequence[stepIndex][1]);
  digitalWrite(IN3, stepSequence[stepIndex][2]);
  digitalWrite(IN4, stepSequence[stepIndex][3]);

  if (stepperCW) {
    stepIndex = (stepIndex + 1) % 8;
  } else {
    stepIndex = (stepIndex - 1 + 8) % 8;
  }
}  // end stepperStep()

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;

  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    String msg = String((char *)data);
    DynamicJsonDocument doc(200);
    deserializeJson(doc, msg);

    String type = doc["type"];
    auto value = doc["value"];

    if (type == "servoAngle") {
      targetServoAngle = value;
    } else if (type == "stepperSpeed") {
      stepDelay = value;
    } else if (type == "stepperDir") {
      stepperCW = (value == "CW");
    }
  }
}  // WebSocket Handler

void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  myServo.attach(SERVO_PIN);
  myServo.write(currentServoAngle);

  // Start WiFi AP
  WiFi.softAP(ssid, password);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  // Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", webpage);
  });

  // WebSocket
  webSocket.begin();
  webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_TEXT) {
      handleWebSocketMessage(&num, payload, length);
    }
  });

  server.begin();
}  // end setup()

void loop() {
  webSocket.loop();

  // Servo motion with constant speed
  if (currentServoAngle < targetServoAngle) {
    currentServoAngle++;
    myServo.write(currentServoAngle);
    delay(SERVO_STEP_DELAY);
  }

  else if (currentServoAngle > targetServoAngle) {
    currentServoAngle--;
    myServo.write(currentServoAngle);
    delay(SERVO_STEP_DELAY);
  }

  // Send servo angle update
  StaticJsonDocument<100> doc;
  doc["type"] = "servo";
  doc["value"] = currentServoAngle;
  String jsonStr;
  serializeJson(doc, jsonStr);
  webSocket.broadcastTXT(jsonStr);

  // Stepper motion
  stepperStep();
  delay(stepDelay);
}  // end loop
