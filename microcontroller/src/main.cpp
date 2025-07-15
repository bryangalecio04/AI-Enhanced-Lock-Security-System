#include <Arduino.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#define MAX_PASSWORD_LENGTH (20)
#define STRING_TERMINATOR '\0'

class Password{
public:
  Password(char* pass){set(pass); reset();}
  void set(char* pass){target = pass;}
  bool is(char* pass);
  bool append(char character);
  void reset();
  bool evaluate();
  Password& operator=(char* pass);
  bool operator==(char* pass);
  bool operator!=(char* pass);
  Password& operator<<(char character);

private:
  char* target;
  char guess[MAX_PASSWORD_LENGTH];
  byte currentIndex;
};

bool Password::is(char* pass){
  byte i = 0;

  while(*pass && i < MAX_PASSWORD_LENGTH){
    guess[i] = pass[i];
    i++;
  }
  return evaluate();
}

bool Password::append(char character){
  if(currentIndex + 1 == MAX_PASSWORD_LENGTH){
    return false;
  }
  else{
    guess[currentIndex++] = character;
    guess[currentIndex] = STRING_TERMINATOR;
  }
  return true;
}

void Password::reset(){
  currentIndex = 0;
  guess[currentIndex] = STRING_TERMINATOR;
}

bool Password::evaluate(){
  char pass = target[0];
  char guessed = guess[0];

  for(byte i = 1; i < MAX_PASSWORD_LENGTH; ++i){
    if((STRING_TERMINATOR == pass) && (STRING_TERMINATOR == guessed)){
      return true;
    }
    else if((pass != guessed) || (STRING_TERMINATOR == pass) || (STRING_TERMINATOR == guessed)){
      return false;
    }
    pass = target[i];
    guessed = guess[i];
  }

  return false;
}

Password& Password::operator=(char* pass){
  set(pass);
  return *this;
}

bool Password::operator==(char* pass){
  return is(pass);
}

bool Password::operator!=(char* pass){
  return !is(pass);
}

Password& Password::operator<<(char character){
  append(character);
  return *this;
}

#define raspberryPI 9
#define buzzer 11
#define GREEN 12
#define RED 13
Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2); // SDA(A4) SCL(A5) 

Password password = Password("012345"); // Default Password

int currentPasswordLength = 0;
int maxPasswordLength = 6;
int a = 5;

bool value = true;
bool isSafeOpen = false;
bool isConfirmingOldPassword = false;
bool isChangingPassword = false;
bool isReadingFace = false;
int unauthorizedCount = 0;

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
  for (int i = 0; i < maxPasswordLength; i++) {
    EEPROM.write(i, newPassword[i]); // Save each character in EEPROM
  }
  EEPROM.write(maxPasswordLength, STRING_TERMINATOR); // Add null terminator
}

void loadPasswordFromEEPROM(char* passwordBuffer){
  for (int i = 0; i < maxPasswordLength; i++) {
    passwordBuffer[i] = EEPROM.read(i); // Load each character from EEPROM
  }
  passwordBuffer[maxPasswordLength] = STRING_TERMINATOR; // Ensure null termination
}

void setup(){
pinMode(raspberryPI, INPUT);
pinMode(buzzer, OUTPUT);
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

servo.attach(10);
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
  password.reset();
  currentPasswordLength = 0;
  a = 5;
  lcd.clear();
}

void lcdReset(){clear(); lcd.setCursor(0,0);}

void passwordConfirmed(){isConfirmingOldPassword = false;}

void passwordChanged(){isChangingPassword = false;}

void faceRead(){isReadingFace = false;}

void openToChangePassword(){
  delay(60);
  clear();
  lcd.setCursor(2,0);
  lcd.print("OPEN SAFE TO");
  lcd.setCursor(0,1);
  lcd.print("CHANGE PASSWORD!");
  delay(3000);
  clear();
}

void wrongPassword(){
  delay(60);
  lcdReset();
  lcd.print("WRONG PASSWORD!");
  lcd.setCursor(0, 1);
  lcd.print("PLEASE TRY AGAIN");
  for(int i = 0; i < 5; i++){
    digitalWrite(buzzer, HIGH); digitalWrite(RED, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW); digitalWrite(RED, LOW);
    delay(200);
  }
  delay(3000);
  clear();
}

void openSafe(){
  value = false;
  isSafeOpen = true;
  digitalWrite(buzzer, HIGH); digitalWrite(GREEN, HIGH);
  delay(300);
  digitalWrite(buzzer, LOW); digitalWrite(GREEN, LOW);
  servo.write(180);
  delay(100);
  delay(3000);
  clear();
}

void lockSafe(){
  value = true;
  isSafeOpen = false;
  digitalWrite(buzzer, HIGH); digitalWrite(GREEN, HIGH);
  delay(300);
  digitalWrite(buzzer, LOW); digitalWrite(GREEN, LOW);
  servo.write(0);
  delay(100);
  delay(3000);
  clear();
}

void safeOpened(){
  delay(60);
  if(password.evaluate()){
    lcdReset();
    lcd.print("CORRECT PASSWORD");
    lcd.setCursor(0, 1);
    lcd.print("SAFE OPENED");
    openSafe();
  }
  else{
    wrongPassword();
  }
  clear();
}

void safeLocked(){
  delay(60);
  lcdReset();
  lcd.print("SAFE LOCKED");
  lockSafe();
}

void processNumberKey(char key){
  delay(60);
  if(key == '*'){
    return;
  }

  lcd.setCursor(a, 1);
  lcd.print(key);
  a++;
  currentPasswordLength++;
  if (a == 11) a = 5;
  password.append(key);

  if(currentPasswordLength > maxPasswordLength){
    lcdReset();
    lcd.print("MAX 6 CHARACTERS");
    delay(3000);
    clear();
  }
}

void confirmPassword(char key){
  delay(60);
  static String oldPass = "";
  static char lastKey = NO_KEY;

  if((key != '*') && (key != '#')){
    lcd.setCursor(a, 1);
    lcd.print(key);
    a++;
    currentPasswordLength++;
    if (a == 11) a = 5;
    password.append(key);
    oldPass += key;
  }
  if((lastKey == '*') && (key == '*')){
    oldPass = "";
    clear();
    lastKey = NO_KEY;
  }
  else if((lastKey == '*') && (key == '#')){
    oldPass = "";
    clear();
    passwordChanged();
    lastKey = NO_KEY;
  }
  else if((lastKey != '*') && (key == '#')){
    if(password.evaluate()){
      digitalWrite(buzzer, HIGH); digitalWrite(GREEN, HIGH);
      delay(300);
      digitalWrite(buzzer, LOW); digitalWrite(GREEN, LOW);
      oldPass = "";
      clear();
      passwordConfirmed();
    }
    else{
      oldPass = "";
      wrongPassword();
    }
    lastKey = NO_KEY;
  }
  if(currentPasswordLength > maxPasswordLength){
    oldPass = "";
    lcdReset();
    lcd.print("MAX 6 CHARACTERS");
    delay(3000);
    clear();
  }
  lastKey = key;
}

void changePassword(char key){
  delay(60);
  if(!isConfirmingOldPassword){
    static String newPass = "";
    static char lastKey = NO_KEY;

    if((key != '*') && (key != '#')){
      lcd.setCursor(a, 1);
      lcd.print(key);
      a++;
      currentPasswordLength++;
      if (a == 11) a = 5;
      password.append(key);
      newPass += key;
    }
    if((lastKey == '*') && (key == '*')){
      newPass = "";
      clear();
      lastKey = NO_KEY;
    }
    else if((lastKey == '*') && (key == '#')){
      newPass = "";
      clear();
      passwordChanged();
      lastKey = NO_KEY;
    }
    else if((lastKey != '*') && (key == '#')){
      if(newPass.length() < 4){
        newPass = "";
        lcdReset();
        lcd.print("MIN 4 CHARACTERS");
        delay(3000);
        clear();
      }
      else if(newPass.length() >= 4){
        char newPassword[maxPasswordLength];
        newPass.toCharArray(newPassword, newPass.length()+1);
        password = Password(newPassword);
        savePasswordToEEPROM(newPassword);
        newPass = "";
        lcdReset();
        lcd.print("PASSWORD UPDATED");
        digitalWrite(buzzer, HIGH); digitalWrite(GREEN, HIGH);
        delay(300);
        digitalWrite(buzzer, LOW); digitalWrite(GREEN, LOW);
        delay(100);
        delay(3000);
        clear();
        passwordChanged();
      }
    }
    if(currentPasswordLength > maxPasswordLength){
      newPass = "";
      clear();
      lcd.setCursor(0,0);
      lcd.print("MAX 6 CHARACTERS");
      delay(3000);
      clear();
    }
    lastKey = NO_KEY;
  }
  else{
    confirmPassword(key);
  }
}

void facialRecognition(){
  delay(60);
  int authorization = digitalRead(raspberryPI);
  if(authorization == HIGH){
    authorization = 0;
    unauthorizedCount = 0;
    lcdReset();
    lcd.print("FACE RECOGNIZED");
    lcd.setCursor(0, 1);
    lcd.print("SAFE OPENED");
    openSafe();
    faceRead();
    char storedPassword[maxPasswordLength];
    loadPasswordFromEEPROM(storedPassword);
    password = Password(storedPassword);
  }
  else if(authorization == LOW){
    if(unauthorizedCount <= 3){
      unauthorizedCount++;
      delay(1000);
    }
    else{
      unauthorizedCount = 0;
      lcdReset();
      lcd.print("TIMED OUT");
      lcd.setCursor(0, 1);
      lcd.print("ENTER PASSWORD");
      delay(3000);
      clear();
      faceRead();
      char storedPassword[maxPasswordLength];
      loadPasswordFromEEPROM(storedPassword);
      password = Password(storedPassword);
    }
  }
}

void loop(){
  if(!isChangingPassword && !isSafeOpen && !isReadingFace){lcd.setCursor(1, 0); lcd.print("ENTER PASSWORD");}

  else if(!isChangingPassword && isSafeOpen){lcd.setCursor(4,0); lcd.print("PRESS #"); lcd.setCursor(4,1); lcd.print("TO LOCK");}

  else if(isChangingPassword && !isConfirmingOldPassword){lcd.setCursor(1, 0); lcd.print("ENTER NEW PASS");}

  else if(isChangingPassword && isConfirmingOldPassword){lcd.setCursor(1,0); lcd.print("ENTER OLD PASS");}

  else if(isReadingFace){lcd.setCursor(0,0); lcd.print("Scanning Face...");}

  char key = keypad.getKey();
  static char lastKey = NO_KEY;

  if((key != NO_KEY) && !isChangingPassword && !isReadingFace){
    delay(60);
    if (lastKey == '*' && key== '*'){
      clear();
      lastKey = NO_KEY;
    }
    else if(lastKey == '*' && key =='#' && isSafeOpen){
      isConfirmingOldPassword = true;
      isChangingPassword = true;
      clear();
      lastKey = NO_KEY;
    }
    else if(lastKey == '*' && key == '#' && !isSafeOpen){
      openToChangePassword();
      lastKey = NO_KEY;
    }
    else if(lastKey == '*' && key == '0' && !isSafeOpen){
      isReadingFace = true;
      clear();
      lastKey = NO_KEY;
    }
    else if(key == '#'){
      if(value){
        safeOpened();
      }
      else if(!value){
        safeLocked();
      }
      lastKey = NO_KEY;
    }
    else if(isSafeOpen){
      lastKey = key;
    }
    else if(!isSafeOpen){
      processNumberKey(key);
      lastKey = key;
    }
  }
  else if((key !=NO_KEY) && isChangingPassword){
    changePassword(key);
  }
  else if(isReadingFace){
    facialRecognition();
  }
}