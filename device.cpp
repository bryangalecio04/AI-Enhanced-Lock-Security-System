#include <Arduino.h>
#include <MPU6050_6Axis_MotionApps20.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#define echo 2
#define trig 3
#define buzzer 4
#define yaw 5
#define pitch1 6
#define pitch2 7
#define roll 8
#define Laser 34

LiquidCrystal_I2C lcd(0x27, 16, 2); // SDA(A4) SCL(A5)
MPU6050 mpu;
Servo servoYaw;
Servo servoPitch1;
Servo servoPitch2;
Servo servoRoll;

bool DMPReady = false;
uint8_t status; // return status after each device operation (0 = success)
uint8_t FIFOBuffer[64];
Quaternion q; // [w, x, y, z] quaternion container
VectorFloat gravity; // [x, y, z] gravity vector
float ypr[3]; // [yaw, pitch, roll] in radians

void setup() {
  Serial.begin(9600);
  
  pinMode(trig, OUTPUT);
  pinMode(echo, INPUT);
  pinMode(buzzer, OUTPUT);

  lcd.init();
  lcd.backlight();

  servoYaw.attach(yaw);
  servoPitch1.attach(pitch1);
  servoPitch2.attach(pitch2);
  servoRoll.attach(roll);
  servoYaw.write(90);
  servoPitch1.write(90);
  servoPitch2.write(90);
  servoRoll.write(90);

  mpu.initialize();
  status = mpu.dmpInitialize();

  mpu.setXGyroOffset(0);  //Set your gyro offset for axis X
  mpu.setYGyroOffset(0);  //Set your gyro offset for axis Y
  mpu.setZGyroOffset(0);  //Set your gyro offset for axis Z
  mpu.setXAccelOffset(0); //Set your accelerometer offset for axis X
  mpu.setYAccelOffset(0); //Set your accelerometer offset for axis Y
  mpu.setZAccelOffset(0); //Set your accelerometer offset for axis Z

  if(mpu.testConnection() && status == 0) {
    Serial.println("MPU6050 and DMP connection successful");
    mpu.CalibrateGyro(6);
    mpu.CalibrateAccel(6);
    mpu.setDMPEnabled(true);
    DMPReady = true;
  }
  else {
    Serial.println("MPU6050 and DMP onnection failed");
  }
}

void buzzer_on() {
  digitalWrite(buzzer, HIGH);
  delay(200);
  digitalWrite(buzzer, LOW);
  delay(200);
}

void ultra_sonic_sensor() {
  long duration, distance;

  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  // Read the echoPin, which returns the sound wave travel time in microseconds
  duration = pulseIn(echo, HIGH); // pulseIn() returns in microseconds
  distance = (duration * 0.034)/ 2; // (duration (microseconds) * speed of sound (10^-4 m/s)) / 2 computes distance in cm
  lcd.setCursor(0, 0);
  lcd.print("Distance: " + String(distance) + " cm");
  Serial.println("Distance: " + String(distance) + " cm");
  if(distance < 25) { // If the distance is less than 25 cm, an object is detected
    buzzer_on();
  }
}

void aircraftStabilizer() {
  static int lastYaw = 90;
  static int lastPitch1 = 90;
  static int lastPitch2 = 90;
  static int lastRoll = 90;

  mpu.dmpGetQuaternion(&q, FIFOBuffer); // Extract quaternion from FIFO buffer (orientation data from DMP)
  mpu.dmpGetGravity(&gravity, &q); // Calculate gravity vector from quaternion
  mpu.dmpGetYawPitchRoll(ypr, &q, &gravity); // Calculate yaw, pitch, and roll angles in radians from quaternion and gravity vector
  float yawDegree = ypr[0] * 180/M_PI;
  float pitchDegree = ypr[1] * 180/M_PI;
  float rollDegree = ypr[2] * 180/M_PI;
  Serial.println("ypr: " + String(yawDegree) + " " + String(pitchDegree) + " " + String(rollDegree));

  int targetYaw = (yawDegree < -10  || yawDegree > 10) ? (90 + yawDegree) : 90;
  if(targetYaw != lastYaw) {
    servoYaw.write(targetYaw);
    delay(200);
    lastYaw = targetYaw;
  }

  int targetPitch1 = (pitchDegree < -10  || pitchDegree > 10) ? (90 + pitchDegree) : 90;
  int targetPitch2 = (pitchDegree < -10  || pitchDegree > 10) ? (90 - pitchDegree) : 90;
  if(targetPitch1 != lastPitch1 && targetPitch2 != lastPitch2) {
    servoPitch1.write(targetPitch1);
    servoPitch2.write(targetPitch2);
    delay(200);
    lastPitch1 = targetPitch1;
    lastPitch2 = targetPitch2;
  }

  int targetRoll = (rollDegree < -10  || rollDegree > 10) ? (90 - rollDegree) : 90;
  if(targetRoll != lastRoll) {
    servoRoll.write(targetRoll);
    delay(200);
    lastRoll = targetRoll;
  }
}

void loop() {
  ultra_sonic_sensor();
  if(DMPReady && mpu.dmpGetCurrentFIFOPacket(FIFOBuffer)) {
    aircraftStabilizer();
  }
}
