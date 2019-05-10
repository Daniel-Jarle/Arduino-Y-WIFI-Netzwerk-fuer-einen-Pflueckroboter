/*
  UDP Bidirectional Commmunication for server with three clients, which not all have to be present.
  No change necessary.
  For the definition of the AT commands, see for
  https://room-15.github.io/blog/2015/03/26/esp8266-at-command-reference/#at-commands
  For the definitions of the comands, see for text file: "list of commands"
  
  The serial monitor has to be set to Baud 19200 and CR and LF active.
  Messages to be send or received by ESP or entered via Serial Monitor have the following syntax:
  <message Msg> : <char original sender Id on send or final receiver Id on receive>
  example: 100pv:0
  distance in dm
*/

#include <SoftwareSerial.h>

//network
SoftwareSerial esp8266(11, 12); // RX, TX

#define DEBUG true //setting debug mode to true

String me = "5"; //own Id
String frId = me; //final receivers Id on receiving a message
String osId = me; //original senders Id on sending a message

String Msg; //net message
bool ESPsga = true; //ESPsga (ESP self generated answer) means the incoming is supposed to be a ESP self generated answer, true for the beginning


//flags

bool calflag = false; //indicates if calibration is in progress, the offsets are deactivated, when calflag == 1
bool fa1 = false, fb1 = false, fa2 = false, fb2 = false, reqflag = false, demoflag = false; // flags for alpha1 and so on, indicating that the respective value has been received

//calculation
#define pi 3.14159

//long atime = millis(); //courrently not used
byte pa1 = 30, pa2 = 60, pb1 = 30, pb2 = 45; //angles alpha and beta to be received from the clients, the start values result in x=A/4, y= (A/4)*sq(3), z=A/2
byte sa1, sa2, sb1, sb2;                     //angles alpha and betta
int oa1 = 0, oa2 = 0, ob1 = 0, ob2 = 0;      //offsets: calibrated - received, i.e. ca1 - pa1
byte ca1, ca2, cb1, cb2;                     //the angles alpha and beta as computed from s1, s2 and zc are the calibration values

double A = 4.3;                              //distance between the goniometers
float x = 0, y = 0, z = 0, Number = 0;       //coordinates of the fruit
double s1 = 8, s2 = 8, zc = 0.4;             //A devided by squareroot of 2 forming a square with A as diagonal, start value make all angles 45°


void setup() {
  Serial.begin(19200);                      

  //network
  esp8266.begin(19200);                     //starts a SofrwareSerial link with esp8266 object on 19200 baut rate
  esp8266.setTimeout(5000);
  if (sendCom("AT+RST", "ready")) debug("RESET OK");  //retets esp module
  else debug("RESET NOT OK");

  if (configAP()) {                                   //configurate AP
    debug("AP ready");
  }
  if (configUDP()) {                                  //configurate UDP server
    debug("UDP ready");
  }

  Serial.println("goniometerdistance in dm =  " + (String)A);
  Serial.println("to calibrate measure and input s1, s2 and zc\n than set calflag with the syntax \"1cf\"");
}

void loop() {
  //networrk
  String Sender = me;
  char InChar;
  while (esp8266.available()) {
    if (ESPsga) {//if assumed, that it is ESP self generated answer, check it
      InChar = esp8266.read();  //reads the message from the esp module
      if (InChar == '+') {      
        esp8266.find("IPD,");
        delay(1);
        //read original sender Id: osId
        osId = (String)esp8266.parseInt();
        esp8266.find(':');
        //clear the net message: Msg
        Msg = "";
        //Found "+IPD,", so we set ESPsga false to not read it again until next message or answer
        ESPsga = false;//it is not a ESP self generated answer, so it is a message
      } else
        //clear buffer, when "+IPD," not found
        esp8266.find('\n');
    } else {//it is clear, that a message has been found
      //reads one char of the message per looping from the buffer
      InChar = esp8266.read();

      if (!(InChar == ':')) {
        Msg += InChar;//add net message together, when inChar not ':'
      } else {
        //read final receiver Id: frId
        frId = (String)esp8266.parseInt();

        //Find out who was the original sender
        switch (osId.toInt()) {
          case 0: Sender = "Computer"; break;
          case 1: Sender = "Client 1";   break;
          case 2: Sender = "Client 2";   break;
          default: Sender = "not valid sender";
        }
        //print 'Msg'
        if (frId != me)debug("Msg got: \"" + Msg + "\" from " + Sender);

        //if the message is for me, execute it, else send it to final receiver Id: frId
        if (frId == me) {
          Serial.println("Msg for me:   \"" + Msg + "\"   from:   " + Sender);
          decodeMsg(); esp8266.find('\n');
        }
        //when final receiver is existing, append to message Msg the original sender Id: osId and send it to final receiver
        else if (frId == "0" || frId == "1" || frId == "2")translateMsg(), WifiSend(Msg + ":" + osId, frId);
        else debug("final receiver Id is not valid");

        //clear buffer because usable message ended
        esp8266.find('\n');
        //Message ended, ready for next receive, which is again supposed first to be a ESP self generated answer
        ESPsga  = true;
      }
    }
  }
  // serial
  while (Serial.available()) {
    //identify myself
    osId = me;
    //read message Msg

    Msg = Serial.readStringUntil(':');
    delay(1);
    //to do: parse Msg
    //when Msg does not end with "\r\n", there is more, identify the final receiver Id: frId, default it is me
    frId = me;
    //when Msg ends with "\r\n", get rid of it
    if (Msg.endsWith("\r\n")) {
      Msg = Msg.substring(0, Msg.length() - 2);
    }
    else frId = (String)(Serial.read() - '0');//read the final receiver Id: frId
    //if the message is for me execute it, else send it to final receiver Id: frId
    if (frId == me) {
      debug("Msg for me: \"" + Msg + "\" from: my Serial Monitor");
      decodeMsg();
    }
    //when final receiver is existing, append to message Msg the original sender Id: osId and send it
    else if (frId == "0" || frId == "1" || frId == "2")
      translateMsg(), WifiSend(Msg + ":" + osId, frId);// translateMsg() adapts position values to C1 an C2
    else debug("final receiver Id is not valid");
    Serial.find('\n');//clear buffer
  } // else: computing and other code

  //calculation
// calculations for callibration, whan all angles are recived
  if ((fa1 && fb1 && fa2 && fb2 && calflag) == true) {
    computeangles();// command "ca" supposed, that s1, s2, zc have given before
    computeoffsets();// command "co"
    computecoord();// command "cc"
    calflag = false;
    fa1 = false;
    fb1 = false;
    fa2 = false;
    fb2 = false;
  }
  // calcultations for
  if (fa1 && fb1 && fa2 && fb2 && reqflag ) {
    computecoord();// command "cc"
    reqflag = false;
    fa1 = false;
    fb1 = false;
    fa2 = false;
    fb2 = false;
  }
  // calculations and commands for demmomode
  if (fa1 && fb1 && fa2 && fb2 && demoflag) {
    computecoord();
    sc("2", "1");
    delay(10);
    sc("2", "2");
    fa1 = false;
    fb1 = false;
    fa2 = false;
    fb2 = false;

  }

}

//-----------------------------------------Config ESP8266------------------------------------

boolean configAP() {
  boolean success = true;
  //set  AP mode (host)
  success &= (sendCom("AT+CWMODE=2", "OK"));
  //set AP to SSID "NanoESP" without password "", channel Id 5 and no encoding
  success &= (sendCom("AT+CWSAP=\"NanoESP\",\"\",5,0", "OK"));
  //Set ip addr of ESP8266 softAP
  success &= (sendCom("AT+CIPAP=\"192.168.4.1\"", "OK"));

  return success;
}

boolean configUDP()
{
  boolean success = true;
  //set transfer mode to normal transmission.
  success &= (sendCom("AT+CIPMODE=0", "OK"));
  //set to multiple connections (up to four)
  success &= (sendCom("AT+CIPMUX=1", "OK"));
  //start connection in UDP with assigned IP addresses, send channel and receive channel
  success &= sendCom("AT+CIPSTART=0,\"UDP\",\"192.168.4.2\",90,91", "OK"); //UDP Bidirectional and Broadcast to Packet sender or TWedge
  success &= sendCom("AT+CIPSTART=1,\"UDP\",\"192.168.4.3\",92,93", "OK"); //UDP Bidirectional and Broadcast to Client 1
  success &= sendCom("AT+CIPSTART=2,\"UDP\",\"192.168.4.4\",94,95", "OK"); //UDP Bidirectional and Broadcast to Client 2
  return success;
}

//-----------------------------------------------Control ESP-----------------------------------------------------
// sending a AT-Command to the esp module
boolean sendCom(String command, char respond[])
{
  esp8266.println(command);
  if (esp8266.findUntil(respond, "ERROR"))
  {
    return true;
  }
  else
  {
    debug("ESP SEND ERROR: " + command);
    return false;
  }
}


//-------------------------------------------------Debug Functions------------------------------------------------------
// Serial debugging output
void debug(String Msg)
{ // if debug = false, then there is noc serial output
  if (DEBUG)
  {
    Serial.println(Msg);
  }
}
//---------------------------------------------------------Sender--------------------------------------------------------
// sending a message to an other member of the network
void WifiSend (String Msg, String Id) {
  bool sucess = sendCom("AT+CIPSEND=" + Id + "," + (String)(Msg.length()), "OK");
  delay(1);

  sucess &= sendCom(Msg, "OK");

  if (sucess)
    Serial.print("message send ok: \"" + Msg + "\"");
  switch (Id.toInt()) {
    case 0: Serial.println("   sent to Computer"); break;
    case 1: Serial.println("   sent to Client 1"); break;
    case 2: Serial.println("   sent to Client 2"); break;
    default: Serial.println("   invalid ID");
  }
}

//==================================================translatong Messages============================================================
/* genneral function to add new funktionallity to the system
   funktionallity has its own shortcut, which will then be used to trigger it
   either in the console or in the program or from a message from an other member of the network */
void decodeMsg(void) {
  Number = Msg.toFloat();
  //commands
  // correct incoming and addressed values from C1 and C2 with respect to positions
  if (Msg.endsWith("ph")) { 
    if (osId == "1") {        // saves the value to the according angle
      pa1 = Number;
      if (!calflag)pa1 += oa1;
      fa1 = true;
      Serial.println("pa1 = " + (String)pa1);
      return;
    }
    if (osId == "2") {        
      pa2 = 180 - Number;
      if (!calflag)pa2 += oa2;
      fa2 = true;
      Serial.println("pa2 = " + (String)pa2);
      return;
    }
  }
  if (Msg.endsWith("pv")) {
    if (osId == "1") {
      pb1 = Number - 90;
      if (!calflag)pb1 += ob1;
      fb1 = true;
      Serial.println("pb1 = " + (String)pb1);
      return;
    }
    if (osId == "2") {
      pb2 = Number - 90;
      if (!calflag)pb2 += ob2;
      fb2 = true;
      Serial.println("pb2 = " + (String)pb2);
    }
  }

  if (Msg.endsWith("zc")) { //imputs a callibration value for z
    zc = Number;
    Serial.println("zc = " + (String)zc);
    return;
  }
  if (Msg.endsWith("s1")) { //inputs a value for s1
    s1 = Number;
    Serial.println("s1 = " + String(s1));
    return;
  }
  if (Msg.endsWith("s2")) { //inputs a value for s2
    s2 = Number;
    Serial.println("s2 = " + (String)s2);
    return;
  }
  if (Msg.endsWith("x")) { //inputs a value for x
    x = Number;
    computeangles();
    sendangles();
  }
  if (Msg.endsWith("y")) { //inputs a value for y
    y = Number;
    computeangles();
    sendangles();
  }
  if (Msg.endsWith("z")) { //inputs a value for z
    z = Number;
    computeangles();
    sendangles();
  }
  if (Msg.endsWith("ca")) { //starts computing of angles
    computeangles();
    return;
  }
  if (Msg.endsWith("co")) {// compute offsets
    computeoffsets();
    return;
  }

  if (Msg.endsWith("cc")) {// compute cartesian coordinates
    computecoord();
    return;
  }
  if (Msg.endsWith("cf")) {// starts calibration scan on both eyes
    calflag = Number;
    if (calflag) {
      sc("2", "1");
      delay(1);
      sc("2", "2");
    }
    Serial.println("calibration status active = " + (String)calflag);
    return;
  }
  if (Msg.endsWith("sf")) {// starts san on both eyes
    sc("2", "1");
    delay(1);
    sc("2", "2");
    reqflag = true;
    Serial.println("scan request " + (String)reqflag);
  }
  if (Msg.endsWith("df")) { //starts demo ////
    demoflag = !demoflag;
  }
  if (Msg.endsWith("st")) { //stops all actions on clients
    Msg = "st";
    frId = "1";
    WifiSend(Msg + ":" + osId, frId);
    delay(100);
    frId = "2";
    WifiSend(Msg + ":" + osId, frId);

    demoflag = false;
    calflag = false;
    fa1 = false;
    fb1 = false;
    fa2 = false;
    fb2 = false;
    reqflag = false;
  }
  
  if (Msg.endsWith("rc")) { // rests goniometer to their starting positon /////
    frId = "1";
    Msg = (String)(90 + oa1) + "ph";
    WifiSend(Msg + ":" + osId, frId);
    delay(100);
    frId = "2";
    Msg = (String)(90 + oa1) + "ph";
    WifiSend(Msg + ":" + osId, frId);
    delay(100);

    frId = "1";
    Msg = (String)(0 + ob1) + "pv";
    WifiSend(Msg + ":" + osId, frId);
    delay(100);
    frId = "2";
    Msg = (String)(0 + ob1) + "pv";
    WifiSend(Msg + ":" + osId, frId);
    delay(100);
  }
}

//network
void translateMsg(void) {// adapts the internal values of the server or C0 to the values as understood by C1 and C2
  Number = Msg.toFloat();
  if (Msg.endsWith("ph")) {
    if (frId == "1") {
      if (!calflag) Msg = (String)(int)(Number - oa1) + "ph";
      else Msg = (String)(int)Number + "ph";
    } else if (frId ==  "2") {
      if (!calflag) Msg = (String)(int)(180 - (Number - oa2)) + "ph";
      else Msg = (String)(int)(180 - Number) + "ph";
    }
    return;
  }
  if (Msg.endsWith("pv")) {
    if (frId == "1") {
      if (!calflag) Msg = (String)(int)(Number + 90 - ob1) + "pv";
      else Msg = (String)(int)(Number + 90) + "ph";
    }
    else if (frId ==  "2") {
      if (!calflag) Msg = (String)(int)(Number + 90 - ob2) + "pv";
      else Msg = (String)(int)(Number + 90) + "ph";
    }
    return;
  }
}
void sendangles(void) { // sending angles from server computed from x, y, z to C1 and C2
  osId = "5";
  frId = "1";
  Msg = (String)sa1 + "ph";
  translateMsg();
  WifiSend(Msg + ":" + osId, frId);
  delay(100);
  frId = "2";
  Msg = (String)sa2 + "ph";
  translateMsg();
  WifiSend(Msg + ":" + osId, frId);
  delay(100);
  frId = "1";
  Msg = (String)sb1 + "pv";
  translateMsg();
  WifiSend(Msg + ":" + osId, frId);
  delay(100);
  frId = "2";
  Msg = (String)sb2 + "pv";
  translateMsg();
  WifiSend(Msg + ":" + osId, frId);
}
void sc(String interval, String frId) {
  Msg = interval + "sc";
  WifiSend(Msg + ":" + osId, frId);
  delay(100);
}

//==================================================Computing=================================================================================
void computecoord(void) {// computes the cartesian coordinates out of the angles corrected with offset
  float ap1, bp1, ap2, bp2;
  if (calflag)ap1 = ca1 * pi / 180; else if (reqflag) ap1 = pa1 * pi / 180; else ap1 = sa1 * pi / 180;
  if (calflag)bp1 = cb1 * pi / 180; else if (reqflag) bp1 = pb1 * pi / 180; else bp1 = sb1 * pi / 180;
  if (calflag)ap2 = ca2 * pi / 180; else if (reqflag) ap2 = pa2 * pi / 180; else ap2 = sa2 * pi / 180;
  if (calflag)bp2 = cb2 * pi / 180; else if (reqflag) bp2 = pb2 * pi / 180; else bp2 = sb2 * pi / 180;

  y = A * sin(ap1) * sin(ap2) / sin(ap1 + ap2);
  x = A * (cos(ap1) * sin(ap2) / sin(ap1 + ap2) - 0.5);
  z = A * (sin(ap2) / sin(ap1 + ap2)) * tan(bp1);
  Serial.println("x = " + (String)x );
  Serial.println("y = " + (String)y );
  Serial.println("z = " + (String)z );
}
void computeangles(void) {// compute calibration angles from s1, s2 and zc (calibration value of z) or angles from x, y and z
  double ls1, ls2, lzc;
  byte a1, a2, b1, b2;
  if (calflag) { // decide if input values of calibration procedure are taken or x, y and z values
    Serial.println("s1 =   " + (String)s1);
    Serial.println("s2 =   " + (String)s2);
    Serial.println("zc =   " + (String)zc);
    ls1 = s1, ls2 = s2, lzc = zc;
  }
  else {
    ls1 = sqrt(sq(A / 2 + x) + sq(y));
    ls2 = sqrt(sq(A / 2 - x) + sq(y));
    lzc = z;
  }

  a1 = (int)(acos((sq(A) + (sq(ls1) - sq(ls2))) / (2 * A * ls1)) * 180 / pi + 0.5);
  a2 = (int)(acos((sq(A) - (sq(ls1) - sq(ls2))) / (2 * A * ls2)) * 180 / pi + 0.5);
  b1 = (int)(atan(lzc / ls1) * 180 / pi + 0.5);
  b2 = (int)(atan(lzc / ls2) * 180 / pi + 0.5);


  Serial.println("a1 = " + (String)a1);
  Serial.println("a2 = " + (String)a2);
  Serial.println("b1 = " + (String)b1);
  Serial.println("b2 = " + (String)b2);
  if (calflag) ca1 = a1, ca2 = a2, cb1 = b1, cb2 = b2;
  else sa1 = a1, sa2 = a2, sb1 = b1, sb2 = b2;
}
void computeoffsets(void) {
  if (calflag) {
    oa1 = ca1 - pa1;
    oa2 = ca2 - pa2;
    ob1 = cb1 - pb1;
    ob2 = cb2 - pb2;
  }
  Serial.println("oa1 = " + (String)oa1);
  Serial.println("oa2 = " + (String)oa2);
  Serial.println("ob1 = " + (String)ob1);
  Serial.println("ob2 = " + (String)ob2);
}
