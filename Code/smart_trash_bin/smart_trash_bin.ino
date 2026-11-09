/*
 * Smart Automatic Trash Bin
 * -------------------------
 * HC-SR04 ultrasonic sensor detects a hand/object; a servo opens the lid.
 * The lid stays open while something is in range and closes after a hold
 * time once the area is clear. Non-blocking (millis-based) state machine.
 *
 * Controller used in the reconstructed implementation: Arduino Uno.
 * Sensor: HC-SR04 (assumed replacement; original sensor not confirmed).
 *
 * Wiring:
 *   HC-SR04 TRIG -> D7, ECHO -> D8, VCC -> 5V, GND -> GND
 *   Servo signal -> D9, servo power from a separate 5V supply (common GND)
 */

#include <Servo.h>

Servo lidServo;

const int TRIG_PIN  = 7;
const int ECHO_PIN  = 8;
const int SERVO_PIN = 9;

const int CLOSED_ANGLE = 0;
const int OPEN_ANGLE   = 90;

const unsigned int  DETECTION_DISTANCE_CM = 20;
const unsigned long HOLD_OPEN_MS          = 2500;  // stay open after the object leaves
const unsigned long MEASURE_INTERVAL_MS   = 60;    // HC-SR04 needs ~60 ms between pings

enum LidState { CLOSED, OPEN };
LidState state = CLOSED;

unsigned long lastSeenMs   = 0;
unsigned long lastMeasureMs = 0;

// One ultrasonic measurement in cm. Returns 999 if there is no echo
// (nothing within ~5 m, or a timeout).
unsigned int measureDistanceOnce() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);                      // 10 µs trigger pulse
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);  // 30 ms timeout
  if (duration == 0) return 999;

  // Speed of sound ≈ 343 m/s = 0.0343 cm/µs; divide by 2 for the round trip.
  return duration * 0.0343 / 2;
}

// Median of three readings rejects single spurious echoes.
unsigned int measureDistance() {
  unsigned int a = measureDistanceOnce(); delay(10);
  unsigned int b = measureDistanceOnce(); delay(10);
  unsigned int c = measureDistanceOnce();
  if (a > b) { unsigned int t = a; a = b; b = t; }
  if (b > c) { unsigned int t = b; b = c; c = t; }
  if (a > b) { unsigned int t = a; a = b; b = t; }
  return b;
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  lidServo.attach(SERVO_PIN);
  lidServo.write(CLOSED_ANGLE);

  Serial.println(F("time_ms,distance_cm,lid_state"));
}

void loop() {
  unsigned long now = millis();
  if (now - lastMeasureMs < MEASURE_INTERVAL_MS) return;
  lastMeasureMs = now;

  unsigned int distance = measureDistance();
  bool objectPresent = distance <= DETECTION_DISTANCE_CM;

  if (objectPresent) lastSeenMs = now;

  switch (state) {
    case CLOSED:
      if (objectPresent) {
        lidServo.write(OPEN_ANGLE);
        state = OPEN;
      }
      break;

    case OPEN:
      // Keep the lid open while the object is still there; close only after
      // the area has been clear for HOLD_OPEN_MS.
      if (!objectPresent && now - lastSeenMs >= HOLD_OPEN_MS) {
        lidServo.write(CLOSED_ANGLE);
        state = CLOSED;
      }
      break;
  }

  Serial.print(now);
  Serial.print(',');
  Serial.print(distance);
  Serial.print(',');
  Serial.println(state == OPEN ? 1 : 0);
}
