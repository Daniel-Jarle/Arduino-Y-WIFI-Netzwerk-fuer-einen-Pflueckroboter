/*  attach haservo to Pin 2
    attach vservo to Pin 3
    connect voltage and ground for each servo
    connect Pin 9 to the Pin 9 of the master
    connect Pin 10 to Pin 8 of the master to send ready signals
*/
//communication
#include <SoftwareSerial.h>
SoftwareSerial master(9, 10); // RX, TX

String Msg;

//Servo
#include <Servo.h>
int hmin = 90;
int hmax = 140;
int vmin = 70;
int vmax = 120;

Servo hservo;  // create servo object to control a servo
Servo vservo;

// variable to store the servo position
byte posnow;// actual position
byte posoldh = 90;
byte posoldv = 90;

int Number;// number included in Msg
int incflag;// gives sign of movement direction

bool Msgflag = false;// indicates if Msg is valid
bool Stop = false;// stop the motion
bool newValue = false;

char servoflag;// indicates, if horizontal ('h') or vertical ('v') movement is active

void setup() {
  //communication
  Serial.begin(19200);
  Serial.println("monitor ready");
  master.begin(1200);
  master.println("ready");

  //LED
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);

  //Servo
  hservo.attach(2);  // attaches the hservo on pin 2 to the servo object
  vservo.attach(3);  // attaches the vservo on pin 3 to the servo object

  hservo.write(posoldh);
  vservo.write(posoldv);
}

void loop() {
  //communication
  //reading message from master
  while (master.available()) {
    delay(2);
    char c = master.read();
    Msg += c;
    decodeMsg();
    if (!master.available() && !Msgflag) Msg = "";// clear Msg if message is nonsense

    if (Msgflag) Serial.println(" Input was:   " + Msg), Msgflag = false, Msg = "", master.find('\n');
  }

  //reading message from serial monitor
  while (Serial.available()) {
    delay(2);
    char c = Serial.read();
    Msg += c;
    decodeMsg();
    if (!Serial.available() && !Msgflag) Msg = "";// clear Msg if message is nonsense
    //Serial.println(Msg + "    "  + (String)Msgflag);
    if (Msgflag) Serial.println(" Input was:   " + Msg), Msgflag = false, Msg = "", Serial.find('\n');
  }

  //Servo
  if ((posnow != Number) && !Stop) {
    posnow += incflag;
    //checking whether the horizontal or the vertical servo has to be moved
    if (servoflag == 'h') hservo.write(posnow), posoldh = posnow;
    if (servoflag == 'v') vservo.write(posnow), posoldv = posnow;
    delay(30);
  }
  else if (newValue) {  //sending an answer to master that movement is completed
    digitalWrite(13, HIGH);
    master.println("ready");
    Serial.println("ready");
    newValue = false;
    digitalWrite(13, LOW);
  }
}

//-----------------------------------------Subroutines---------------------------------------
// decode Msg
void decodeMsg(void) {
  if (Msg.endsWith("ph")) Number = Msg.toInt(), ServoPH(), Stop = false, Msgflag = true, newValue = true;
  else if (Msg.endsWith("pv")) Number = Msg.toInt(), ServoPV(), Stop = false, Msgflag = true, newValue = true;

  else if (Msg.endsWith("st"))Stop = true, Msgflag = true, newValue = true;
}
void ServoPH(void) {  //moving horizontal servo
  Number = constrain(Number, hmin, hmax), servoflag = 'h';
  Serial.println((String)Number + "posh" + " posold  " + posoldh );
  if (posoldh < Number) incflag = 1;  //moving the servo just one degree at a time
  else incflag = -1;
  posnow = posoldh;
}

void ServoPV(void) {  //moving vertical servo
  Number = constrain(Number, vmin, vmax), servoflag = 'v';
  Serial.println((String)Number + "posv" + " posold  " + posoldv);
  if (posoldv < Number) incflag = 1;  //moving the servo just one degree at a time
  else incflag = -1;
  posnow = posoldv;
}
