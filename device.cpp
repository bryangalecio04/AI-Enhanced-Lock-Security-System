#include <Arduino.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <EEPROM.h>

// Password class for storing, evaluating, and changing password
class Password{
  public:
    Password(const String &__init__){
      password = __init__;
    }
    bool evaluate(const String &guess){
      if(guess == password){
        return true;
      }
      else{
        return false;
      }
    }
    void newPassword(const String &newPassword){
      password = newPassword;
    }
  private:
    String password;
};

Password password("012345"); // Default Password

#define STRING_TERMINATOR '\0'
#define servo_motor 10
#define BUZZER 11
#define GREEN 12
#define RED 13
LiquidCrystal_I2C lcd(0x27, 16, 2); // SDA(A4) SCL(A5) 
Servo servo;

String target = "";
String identification = "";
int currentPasswordLength = 0;
const int maxPasswordLength = 6;
int a = 5;
int attempts = 0;
int attemptsLeft = 2;
bool isFaceIDLocked = false;

const byte rows = 4;
const byte columns = 3;
char keys[rows][columns] =
{{'1', '2', '3'},
{'4', '5', '6'},
{'7', '8', '9'},
{'*', '0', '#'}};
byte rowPins[rows] = {5,6,7,8};
byte columnPins[columns] = {2,3,4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, columnPins, rows, columns);

void savePasswordToEEPROM(const char* newPassword){
  for (int i = 0; i < maxPasswordLength; i++){
    EEPROM.write(i, newPassword[i]); // Save each character in EEPROM
  }
  EEPROM.write(maxPasswordLength, STRING_TERMINATOR); // Add null terminator
}

void loadPasswordFromEEPROM(char* passwordBuffer){
  for (int i = 0; i < maxPasswordLength; i++){
    passwordBuffer[i] = EEPROM.read(i); // Load each character from EEPROM
    if(passwordBuffer[i] == STRING_TERMINATOR){
      return;
    }
  }
}

// Initialize buzzer, LEDs, LCD, serial port, motor, and password
void setup(){
pinMode(BUZZER, OUTPUT);
pinMode(GREEN, OUTPUT);
pinMode(RED, OUTPUT);

lcd.init();
lcd.backlight();
lcd.setCursor(5, 0);
lcd.print("Hello");
lcd.setCursor(5, 1);
lcd.print("Team!");
delay(3000);
lcd.clear();

servo.attach(servo_motor);
servo.write(0);

char storedPassword[maxPasswordLength];
loadPasswordFromEEPROM(storedPassword);

if(storedPassword[0] == 0xFF || storedPassword[0] == STRING_TERMINATOR){
  savePasswordToEEPROM("012345");
}
else{
  password = Password(storedPassword);
}

Serial.begin(9600);
Serial.println(storedPassword);
}

void clear(){
  target = "";
  currentPasswordLength = 0;
  a = 5;
  lcd.clear();
}

void clearBuffer(){
  while(Serial.available() > 0){
    Serial.read();
  }
}

void lcdReset(){clear(); lcd.setCursor(0,0);}

// States of the device
enum State{enteringPassword, open, confirmingOldPassword, changingPassword, faceIdentification};
State currState = enteringPassword;

void openToChangePassword(){
  clear();
  lcd.setCursor(2,0);
  lcd.print("OPEN SAFE TO");
  lcd.setCursor(0,1);
  lcd.print("CHANGE PASSWORD!");
  delay(3000);
  clear();
}

void wrongPassword(){
  lcdReset();
  lcd.print("WRONG PASSWORD!");
  lcd.setCursor(0, 1);
  lcd.print("PLEASE TRY AGAIN");
  for(int i = 0; i < 5; i++){
    digitalWrite(BUZZER, HIGH); digitalWrite(RED, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW); digitalWrite(RED, LOW);
    delay(200);
  }
  delay(3000);
  clear();
}

void wrongFace(){
  for(int i = 0; i < 5; i++){
    digitalWrite(BUZZER, HIGH); digitalWrite(RED, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW); digitalWrite(RED, LOW);
    delay(200);
  }
}

void faceIDLocked(){
  lcdReset();
  lcd.print("FACE ID LOCKED");
  lcd.setCursor(0,1);
  lcd.print("ENTER PASSWORD");
  delay(3000);
  clear();
}

void openSafe(){
  digitalWrite(BUZZER, HIGH); digitalWrite(GREEN, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW); digitalWrite(GREEN, LOW);
  servo.write(180);
  delay(300);
  delay(3000);
  clear();
}

void lockSafe(){
  lcdReset();
  lcd.print("SAFE LOCKED");
  digitalWrite(BUZZER, HIGH); digitalWrite(GREEN, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW); digitalWrite(GREEN, LOW);
  servo.write(0);
  delay(300);
  delay(3000);
  clear();
}

void processKey(char key){
  if(key == '*' || key == '#'){
    return;
  }
  lcd.setCursor(a, 1);
  lcd.print("*");
  target += key;
  currentPasswordLength++;
  a++;
  if(currentPasswordLength > maxPasswordLength){
    lcdReset();
    lcd.print("MAX 6 CHARACTERS");
    delay(3000);
    clear();
  }
}

void enterPassword(char key){
  lcd.setCursor(1, 0);
  lcd.print("ENTER PASSWORD");
  static char lastKey = NO_KEY;
  if(key != NO_KEY){
    if(lastKey == '*' && key == '*'){
      lastKey = NO_KEY;
      clear();
    }
    else if(lastKey == '*' && key == '#'){
      lastKey = NO_KEY;
      openToChangePassword();
    }
    else if(lastKey == '*' && key == '0' && !isFaceIDLocked){
      lastKey = NO_KEY;
      clear();
      currState = faceIdentification;
    }
    else if(lastKey == '*' && key == '0' && isFaceIDLocked){
      faceIDLocked();
    }
    else if(key == '#'){
      if(password.evaluate(target)){
        isFaceIDLocked = false;
        lcdReset();
        lcd.print("CORRECT PASSWORD");
        lcd.setCursor(0, 1);
        lcd.print("SAFE OPENED");
        openSafe();
        currState = open;
      }
      else{
        wrongPassword();
      }
      lastKey = NO_KEY;
    }
    else{
      processKey(key);
    }
    lastKey = key;
  }
}

void safeOpened(char key){
  lcd.setCursor(4,0);
  lcd.print("PRESS #");
  lcd.setCursor(4,1);
  lcd.print("TO LOCK");
  static char lastKey = NO_KEY;
  if(key != NO_KEY){
    if(lastKey == '*' && key == '#'){
      lastKey = NO_KEY;
      clear();
      currState = confirmingOldPassword;
    }
    else if(lastKey != '*' && key == '#'){
      lastKey = NO_KEY;
      lockSafe();
      currState = enteringPassword;
    }
    lastKey = key;
  }
}

void confirmPassword(char key){
  lcd.setCursor(1,0);
  lcd.print("ENTER OLD PASS");
  static char lastKey = NO_KEY;
  if(key != NO_KEY){
    if((lastKey == '*') && (key == '*')){
      lastKey = NO_KEY;
      clear();
    }
    else if((lastKey == '*') && (key == '#')){
      lastKey = NO_KEY;
      clear();
      currState = open;
    }
    else if((lastKey != '*') && (key == '#')){
      if(password.evaluate(target)){
        digitalWrite(BUZZER, HIGH); digitalWrite(GREEN, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW); digitalWrite(GREEN, LOW);
        delay(300);
        clear();
        currState = changingPassword;
      }
      else{
        wrongPassword();
      }
      lastKey = NO_KEY;
    }
    else{
      processKey(key);
    }
    lastKey = key;
  }
}

void changePassword(char key){
  lcd.setCursor(1, 0);
  lcd.print("ENTER NEW PASS");
  static char lastKey = NO_KEY;
  if(key != NO_KEY){
    if((lastKey == '*') && (key == '*')){
      lastKey = NO_KEY;
      clear();
    }
    else if((lastKey == '*') && (key == '#')){
      lastKey = NO_KEY;
      clear();
      currState = open;
    }
    else if((lastKey != '*') && (key == '#')){
      if(target.length() < 4){
        lcdReset();
        lcd.print("MIN 4 CHARACTERS");
        delay(3000);
        clear();
      }
      else if(target.length() >= 4){
        password.newPassword(target);
        char newPassword[maxPasswordLength];
        strcpy(newPassword, target.c_str());
        savePasswordToEEPROM(newPassword);
        lcdReset();
        lcd.print("PASSWORD UPDATED");
        digitalWrite(BUZZER, HIGH); digitalWrite(GREEN, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW); digitalWrite(GREEN, LOW);
        delay(300);
        delay(3000);
        clear();
        currState = open;
      }
    }
    else{
      processKey(key);
    }
    lastKey = key;
  }
}

void confirmIdentity(){
  clearBuffer();
  unsigned long startTime = millis();
  while(millis() - startTime < 10000){
    lcd.setCursor(0,0);
    lcd.print("SCANNING FACE...");
    if(Serial.available() > 0){
      identification = Serial.readStringUntil('\n');
      if(identification == "IDENTIFIED"){
        lcdReset();
        lcd.print("FACE IDENTIFIED");
        lcd.setCursor(0,1);
        lcd.print("SAFE OPENED");
        openSafe();
        clearBuffer();
        identification = "";
        attempts = 0;
        attemptsLeft = 2;
        clear();
        currState = open;
        return;
      }
      else if(identification == "Unknown"){
        attempts ++;
        attemptsLeft --;
        if(attempts >= 2){
          attempts = 0;
          attemptsLeft = 2;
          isFaceIDLocked = true;
          faceIDLocked();
          currState = enteringPassword;
          return;
        }
        lcdReset();
        lcd.print("TRY AGAIN");
        lcd.setCursor(0,1);
        lcd.print("ATTEMPTS LEFT:");
        lcd.setCursor(15,1);
        lcd.print(attemptsLeft);
        wrongFace();
        delay(3000);
        clearBuffer();
        identification = "";
        clear();
        currState = enteringPassword;
        return;
      }
    }
  }
  attempts ++;
  attemptsLeft --;
  if(attempts >= 2){
    attempts = 0;
    attemptsLeft = 2;
    isFaceIDLocked = true;
    faceIDLocked();
    currState = enteringPassword;
    return;
  }
  lcdReset();
  lcd.print("TIMED OUT");
  lcd.setCursor(0,1);
  lcd.print("ATTEMPTS LEFT:");
  lcd.setCursor(15,1);
  lcd.print(attemptsLeft);
  wrongFace();
  delay(3000);
  clear();
  currState = enteringPassword;
}

// Checking state of the device
void loop(){
  char key = keypad.getKey();
  switch(currState){
    case enteringPassword:
      enterPassword(key);
      break;
    case open:
      safeOpened(key);
      break;
    case confirmingOldPassword:
      confirmPassword(key);
      break;
    case changingPassword:
      changePassword(key);
      break;
    case faceIdentification:
      confirmIdentity();
      break;
  }
}