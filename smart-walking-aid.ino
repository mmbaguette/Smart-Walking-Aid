

/*
Authors: Yunkai Zhang, Khai Vo, Ali Mohammed-Ali
Date: March 17, 2026

TODO:
Tests:
- Do both buzzers play at the same time when the closest object is North
- The EAST buzzer has tape on its wires
*/

#include <Servo.h>
#include <SPI.h>
#include "MFRC522.h"
#include <Wire.h> 
#include "LiquidCrystal_I2C.h"

// pins
const int switchPin = A3;
const int buttonPin = 2;
const int buzzer1Pin = 4; // left (W)
const int buzzer2Pin = 5; // right (E)
const int servoPin = 6;

// ultrasonic sensor pins
const int trig_pin = 7;
const int echo_pin = 8;

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);
String userName = ""; // name uploaded from NFC tag to RFID reader
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 columns and 2 rows

// ultrasonic sensor measurements
float timing = 0.0;
float distance = 0.0;
int frequency = 0; //tone of the buzzer based on distance

//buzzer trigger bounds in centimeters
const int max_dist = 400; //4m, lowest frequency
const int min_dist = 20; //20cm, highest frequency

// create servo object
Servo radarServo;
int pos = 45; //current position (NW, N, NE)

// states
bool powerOn = false; 
bool userFallen = false;
bool buttonDown = false;
bool lastFallenState = false; //whether the user fell last time or not

// store the last detected object distances for each direction to decide which direction has the closest object approaching it
int distances[6] = {max_dist, max_dist, max_dist}; // index indicates direction (i=0 is NW).  max_dist: nothing detected/out of scan bounds for direction i, <max_dist cm: an object in direction i was last detected at distance x
int directionID = 0; //index of the direction of the approaching object in distances array (0 for NW, 1 for N, 2 for NE)

void displayNametag() {
  lcd.clear();
  lcd.backlight();
  lcd.setCursor(0, 0);

  if (userName != "") { //name already scanned
    lcd.print("Hi, I'm "); 
    lcd.print(userName);
    lcd.setCursor(0, 1);
    lcd.print("I'm visually impaired..."); 
  } else { //not yet scanned
    lcd.print("Please scan tag");
    lcd.setCursor(0,1);
    lcd.print("to display name");
  }
}

// stop playing buzzers, clear //lcd text, stop turning servo,
void powerSystemOff() { 
  noTone(buzzer1Pin);
  noTone(buzzer2Pin);
  lcd.noBacklight();
}

void powerSystemOn() {
  displayNametag();
}

void setup()
{
  Serial.begin(9600);
  
  // RFID 
  // 1. Physical Reset of the RFID Chip
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  delay(50);

  // 2. Initialize LCD
  lcd.init();

  // 3. Initialize SPI and RFID
  SPI.begin();
  mfrc522.PCD_Init();
  
  // 4. Verification
  Serial.print("RFID Status: 0x");
  Serial.println(mfrc522.PCD_ReadRegister(mfrc522.VersionReg), HEX);

  Serial.println("System Ready - Scan Tag...");

  // power system
  pinMode(buttonPin, INPUT);
  
  // ultrasonic sensor + buzzer
  pinMode(echo_pin, INPUT);
  pinMode(trig_pin, OUTPUT);
  pinMode(buzzer1Pin, OUTPUT);
  pinMode(buzzer2Pin, OUTPUT);
  
  digitalWrite(trig_pin, LOW);
  digitalWrite(buzzer1Pin, LOW);
  digitalWrite(buzzer2Pin, LOW);

  //setup servo motor
  radarServo.attach(servoPin); //attach to pin 9
}

void loop()
{    
  // POWER ON/OFF SYSTEM

  //button was down, then lifted. toggled.
  if (buttonDown && !digitalRead(buttonPin)) {
    powerOn = !powerOn;
    buttonDown = false;
    Serial.println("Button toggled!");

    if (!powerOn) { //if power switched off
      powerSystemOff();
    } else if (powerOn) { //switched back on, especially after falling
      powerSystemOn();
    }
  } //end of toggle if statement
  else if (digitalRead(buttonPin)) { //button is currently pressed
    buttonDown = true;
  }

  // stop this loop and don't retrieve distance
  if (!powerOn) {
    delay(100);
    return;
  }

  // TILT SWITCH LOGIC

  lastFallenState = userFallen; //save the previous state
  int a3read = analogRead(switchPin);
  Serial.print("A3 read ");
  Serial.println(a3read);
  userFallen = a3read < 900; //whether switch is open/not 5V at pin (on a scale of 1024 (5V) to 0 (0V)), aka user has fallen

  if (lastFallenState == false && userFallen) { //if the user was not fallen, but fell just now
    Serial.println("The user fell!");
    lcd.clear();   
    lcd.setCursor(0, 0);
    lcd.print("I fell and I");
    lcd.setCursor(0, 1);
    lcd.print("can't get up! ):");
    noTone(buzzer1Pin);
    noTone(buzzer2Pin);
    powerOn = false;
    return;
  }

  // SERVO/RADAR MOVEMENT

  directionID = pos/45-1;
  Serial.print("Scanning direction (0 NW, 1 N, 2 NE): ");
  Serial.println(directionID);

  radarServo.write(pos); //move servo to 45, 90 or 135 degs (NW, N, NE)
  delay(1000+pos*3); //allow some time for servo to move before starting object detection
  
  // find the next scan direction for the next loop
  if (pos == 135) { // if reached the last direction of sweep...
    pos = 45; // ...move radar back to initial position on next loop
  } else {
    pos += 45; // ...otherwise, make the servo move back to beginning on next loop
  }

  // DISTANCE & DIRECTION DETECTION

  digitalWrite(trig_pin, LOW);
  delay(10);
  
  digitalWrite(trig_pin, HIGH);
  delay(15);
  digitalWrite(trig_pin, LOW);
  
  timing = pulseIn(echo_pin, HIGH);
  distance = (timing * 0.034) / 2; //distance in cm

  /*
  State machine conditions
  - If nothing detected in all three directions (not within scan range), both buzzers off 
  - If something detected, and it's closer than the last detected thing, then play the buzzer for that direction
  
  To accomplish this:
  - Store last detected distance for each direction (NW, N, NE) in an array [300cm, 100cm, 400cm]
  */

  // if within scan range
  if (distance >= min_dist && distance <= max_dist) { //                                 (   20  /    20  )                              (  20  /    4000  ) 
    distances[directionID] = distance; //in the distances array, at the index pos/45-1, update the distance
    
    // Update buzzer noises based on the direction with the closest, detected object

    //find closest distance and corresponding direction
    int closestDistance = max_dist;
    int closestDirectionID = 3; //
    for (int i = 0; i < 3; i++) {
      // Serial.print("Comparing direction ");
      // Serial.println(i);
      // Serial.print("Its distance: ");
      // Serial.println(distances[i]);
      if (distances[i] < closestDistance) {
        closestDistance = distances[i];
        closestDirectionID = i;
      }
    }
    
    // Serial.print("Closest distance: ");
    // Serial.println(closestDistance);
    // Serial.print("Closest direction: ");
    // Serial.println(closestDirectionID);
    Serial.print("Distances: ");
    Serial.print("NW: ");
    Serial.print(distances[0]);
    Serial.print(", ");
    Serial.print("N: ");
    Serial.print(distances[1]);
    Serial.print(", ");
    Serial.print("NE: ");
    Serial.println(distances[2]);

    //find frequency based on distance (f ~ 1/d)
    frequency = 5000*((float)min_dist/(float)closestDistance)+100;  //        in front of your face: f = 5000*(min_dist/distance) = 5000 (1), far away = 5000*(min_dist/distance) = 5000(20/4000) = 5000*0.05 (lowest freq)

    //play appropriate sound
    if (closestDirectionID == 0) { // NW
      tone(buzzer1Pin, frequency);
      noTone(buzzer2Pin);
    } 
    else if (closestDirectionID == 1) { // N
      tone(buzzer1Pin, frequency);
      tone(buzzer2Pin, frequency);
    }
    else if (closestDirectionID == 2) { // NE
      noTone(buzzer1Pin);
      tone(buzzer2Pin, frequency);
    }
    else { //otherwise, nothing is approaching the user
      noTone(buzzer1Pin);
      noTone(buzzer2Pin);
    }

    Serial.print("Frequency: ");
    Serial.print(frequency);
    Serial.print(", ");
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.print(" cm, ");
    Serial.print("Direction: ");
    Serial.println(directionID);
    Serial.println("------------------------------------------------");
  } // end of if statement - scan bounds check
  else { //nothing was detected in this direction
     distances[directionID] = max_dist; //in direction pos/45-1, nothing was detected. too far (or too close)
  }

  // RFID SCANNING 

  // Reset the search state every loop
  if ( ! mfrc522.PICC_IsNewCardPresent()) return;
  if ( ! mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("Tag Detected!");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Reading...");

  byte buffer[36]; 
  byte size = 18;
  String tagContent = "";
  
  // Read Block 4
  MFRC522::StatusCode status1 = mfrc522.MIFARE_Read(4, buffer, &size);
  // Read Block 8
  byte size2 = 18;
  MFRC522::StatusCode status2 = mfrc522.MIFARE_Read(8, &buffer[16], &size2);

  if (status1 == MFRC522::StatusCode::STATUS_OK) {
    bool recording = false;
    
    for (uint8_t i = 0; i < 32; i++) {
      // Look for 'en' header
      if (!recording) {
        if (buffer[i] == 'e' && buffer[i+1] == 'n') {
          recording = true;
          i++; 
          continue; 
        }
        continue; 
      }
      // Collect printable characters
      if (buffer[i] >= 32 && buffer[i] <= 126) {
        tagContent += (char)buffer[i];
      }
    }

    tagContent.trim();
    
    if(tagContent.length() > 0) {
      Serial.println("Success: " + tagContent);
      userName = tagContent; //update the user's name
    } else {
      lcd.print("Format Error");
      Serial.println("Error: 'en' header not found on tag.");
      delay(2000); //show error message for one short moment before redisplaying default text or previously scanned name using displayNametag() below
    }
  } else {
    Serial.println("Read Failed.");
    lcd.clear();
    lcd.print("Read Error");
    delay(2000); //show error message for one moment before redisplaying default text or previously scanned name
  } //end of if scan status OK

  displayNametag(); //display the new name

  // CRITICAL: Halt the card so the reader can find the next one
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}