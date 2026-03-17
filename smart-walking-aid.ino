/*
Authors: Yunkai Zhang, Khai Vo, Ali Mohammed-Ali
Date: March 17, 2026

TODO:
- RFID + LCD system
*/

#include <Servo.h>

// pins
const int switchPin = A3;
const int buttonPin = 2;
const int transistorGate = 9;
const int buzzer1 = 4;
const int buzzer2 = 5; 

// ultrasonic sensor pins
const int trig_pin = 9;
const int echo_pin = 10;

// ultrasonic sensor measurements
float timing = 0.0;
float distance = 0.0;
int frequency = 0; //tone of the buzzer based on distance

//buzzer trigger bounds in centimeters
const int max_dist = 400; //4m, lowest frequency
const int min_dist = 20; //20cm, highest frequency

// create servo object
Servo radarServo;
int pos = 0; //current position (NW, N, NE)
double angle;

// states
bool powerOn = false; 
bool userFallen = false;
bool buttonDown = false;

// stop playing buzzers, clear LCD text, stop turning servo,
void powerSystemOff() {
  noTone(buzzer1);
  noTone(buzzer2);
  lcd.clear();
  lcd.noBacklight();
}

void setup()
{
  //power system
  pinMode(transistorGate, OUTPUT);
  pinMode(buttonPin, INPUT);
  
  //ultrasonic sensor + buzzer
  pinMode(echo_pin, INPUT);
  pinMode(trig_pin, OUTPUT);
  pinMode(buzzer, OUTPUT);
  
  digitalWrite(trig_pin, LOW);
  digitalWrite(buzzer, LOW);

  //setup servo motor
  myservo.attach(9); //attach to pin 9

  Serial.begin(9600);
}

void loop()
{    
  // POWER LOGIC

  //button was down, then lifted. toggled.
  if (buttonDown && !digitalRead(buttonPin)) {
    powerOn = !powerOn;
    buttonDown = false;
    Serial.println("Button toggled!");

    if (!powerOn)
      powerSystemOff();
  }
  else if (digitalRead(buttonPin)) { //button is currently pressed
    buttonDown = true;
  }

  digitalWrite(transistorGate, powerOn);

  // TILT SWITCH LOGIC

  lastFallenState = userFallen; //save the previous state
  userFallen = analogRead(switchPin) < 1000; //whether switch is open, aka user has fallen

  if (lastFallenState == false && userFallen) //if the user was not fallen, but fell just now
    powerSystemOff();

  // stop this loop and don't retrieve distance
  if (!powerOn)
    return;

  // SERVO LOGIC
  pos += 45; //move the radar once each loop over an FOV from 45 to 135 degs. then repeat
  if (pos == 180)
    pos = 0; //move radar back to initial position; will be incremented to 45 degs on next loop
  radarServo.write(pos);
  delay(pos*3); //allow some time for servo to move before scanning

  // DISTANCE + BUZZER LOGIC

  digitalWrite(trig_pin, LOW);
  delay(2);
  
  digitalWrite(trig_pin, HIGH);
  delay(10);
  digitalWrite(trig_pin, LOW);
  
  timing = pulseIn(echo_pin, HIGH);
  distance = (timing * 0.034) / 2; //distance in cm
  
  if (distance >= min_dist && distance <= max_dist) { //                                 (   20  /    20  )                              (  20  /    4000  ) 
    frequency = 5000*(min_dist/distance)+100;  //        in front of your face: f = 5000*(min_dist/distance) = 5000 (1), far away = 5000*(min_dist/distance) = 5000(20/4000) = 5000*0.05 (lowest freq)
  	Serial.print("Frequency: ");
    Serial.print(frequency);
    Serial.print(", ");
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println("");
    tone(buzzer, frequency);

  } else {
  	noTone(buzzer);
  }

  delay(1500);  
}