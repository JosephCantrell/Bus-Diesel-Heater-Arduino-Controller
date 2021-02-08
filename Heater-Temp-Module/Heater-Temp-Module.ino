#include <SoftwareSerialWithHalfDuplex.h>
SoftwareSerialWithHalfDuplex sOne(2, 2); // NOTE TX & RX are set to same pin for half duplex operation
int inhibswitch = 11;
unsigned long lasttime;
String heaterstate[] = {"Off","Starting","Pre-Heat","Failed Start - Retrying","Ignition - Now heating up","Running Normally","Stop Command Received","Stopping","Cooldown"};
int heaterstatenum = 0;
int heatererror = 0;

const byte CLOSE = 1;
const byte OPEN = 0;

const byte ON = 1;
const byte OFF = 0;

byte bedValvePower = 3;
byte bedValveDir = 4;
byte bathValvePower = 5;
byte bathValveDir = 6;
byte kitchenValvePower = 7;
byte kitchenValveDir = 8;
byte fanValvePower = 9;
byte fanValveDir = 10;

byte bedTempPin = 12;
byte bathTempPin = 13;
byte kitchenTempPin = 14;

const float MOVEMENT_TIME = 1.5;

float bedTimer = 0;
float bathTimer = 0;
float kitchenTimer = 0;
float fanTimer = 0;

bool bedMoving = false;
bool bathMoving = false;
bool kitchenMoving = false;
bool fanMoving = false;

bool bedOpen = false;
bool bathOpen = false;
bool kitchenOpen = false;
bool fanOpen = false;

String controlenable = "False";
String inhibseenactive = "False";
float currtemp = 0;
float settemp = 0;
float command = 0;
void setup()
{
  pinMode(inhibswitch,INPUT_PULLUP);
  pinMode(bedValvePower,OUTPUT);
  pinMode(bedValveDir,OUTPUT);
  pinMode(bathValvePower,OUTPUT);
  pinMode(bathValveDir,OUTPUT);
  pinMode(kitchenValvePower,OUTPUT);
  pinMode(kitchenValveDir,OUTPUT);
  pinMode(fanValvePower,OUTPUT);
  pinMode(fanValveDir,OUTPUT);
  
  
  
  // initialize listening serial port
  // 25000 baud, Tx and Rx channels of Chinese heater comms interface:
  // Tx/Rx data to/from heater, special baud rate for Chinese heater controllers
  sOne.begin(25000);
  // initialise serial monitor on serial port 0
  Serial.begin(115200);
  // prepare for detecting a long delay
  lasttime = millis();
  Serial.println("working");
  
}

float getTemp(byte pin, int temp){
  int currentTemp = 0;

  // Get the temp from the sensor here

  if (currentTemp > temp + 1){
    return true;
  }
  else{
    return false;
  }

  
}

void disableMotor(byte powerPin){
  digitalWrite(powerPin, OFF);
}

bool controlValves(byte powerPin, byte dirPin, byte dir){
  digitalWrite(dirPin, dir);
  digitalWrite(powerPin, ON);
  if (dir == CLOSE)
    return false;
  else
    return true;
}

void incrementTimers(){
  if (bedMoving){
    bedTimer = millis();
    if (millis() > bedTimer + (MOVEMENT_TIME * 1000)){
      bedTimer = 0;
      disableMotor(bedValvePower);
      bedMoving = false;
    }
  }
  if (bathMoving){
    bathTimer = millis();
    if (millis() > bathTimer + (MOVEMENT_TIME * 1000)){
      bathTimer = 0;
      disableMotor(bathValvePower);
      bathMoving = false;
    }
  }
  if (kitchenMoving){
    kitchenTimer = millis();
    if (millis() > kitchenTimer + (MOVEMENT_TIME * 1000)){
      kitchenTimer = 0;
      disableMotor(kitchenValvePower);
      kitchenMoving = false;
    }
  }
  if (fanMoving){
    fanTimer = millis();
    if (millis() > fanTimer + (MOVEMENT_TIME * 1000)){
      fanTimer = 0;
      disableMotor(fanValvePower);
      fanMoving = false;
    }
  }
}

void loop()
{
  static byte Data[48];
  static bool RxActive = false;
  static int count = 0;
  
  
  
  // read from serial on D2
  if (sOne.available()) 
  {
    
    // calc elapsed time since last rx’d byte to detect start of frame sequence
    unsigned long timenow = millis();
    unsigned long diff = timenow - lasttime;
    lasttime = timenow;
    
    if(diff > 100) 
    { // this indicates the start of a new frame sequence
      RxActive = true;
    }
    int inByte = sOne.read(); // read hex byte
    if(RxActive) 
    {
      Data[count++] = inByte;
      if(count == 48) 
      {
        RxActive = false;
      }
    }
  }
  
  if(count == 48) { // filled both frames – dump
    count = 0;
    command = int(Data[2]);
    currtemp = Data[3];
    settemp = Data[4];
    heaterstatenum = int(Data[26]);
    heatererror = int(Data[41]);
    
    if (digitalRead(inhibswitch) == HIGH) {
      inhibseenactive = "True";
    }
    
    if (digitalRead(inhibswitch) == LOW && (inhibseenactive == "True")) {
      inhibseenactive = "False";
      Serial.println("Inhibit Switch Toggled disable-enable - Enabling Auto");
      controlenable = "True";
    }
    
    Serial.println();
    Serial.print("Command ");
    Serial.println(int(command));
    Serial.println(heaterstate[heaterstatenum]);
    Serial.print("Error Code ");
    Serial.println(heatererror);
    Serial.print("Current Temp ");
    Serial.println(int(currtemp));
    Serial.print("Set Temp ");
    Serial.println(int(settemp));
    Serial.print("System Enabled : ");
    Serial.println(controlenable);

    
    if (int(command) == 160) {
      Serial.println("Start command seen from controller - Enabling Auto");
      // Dont check if the fan valve is moving. Regardless we need to send a close signal ( if the program starts up thinking the valve is closed but it really isnt we can melt the fan )
      fanOpen = controlValves(fanValvePower, fanValveDir, CLOSE);
      fanMoving = true;
      controlenable = "True";
    }
    if (int(command) == 5) {
      Serial.println("Stop command seen from controller - Disabling Auto");
      controlenable = "False";
    }
    // Standard Range     Temp +2 to Temp -1 (3 degrees)
    

    // This is where the valve controls should occur. 
    if (heaterstatenum == 5){
      // Check bedroom temp pin. if true we need to close the valve
      if (getTemp(bedTempPin, int(settemp))){
        // Only close the valve if the valve is already open
        if (bedOpen){
          bedOpen = controlValves(bedValvePower, bedValveDir, CLOSE);
          bedMoving = true;
        }
      }
      else{
        // Only open the valve if the valve is already closed
        if (!bedOpen){
          bedOpen = controlValves(bedValvePower, bedValveDir, OPEN);
          bedMoving = true;
        }
      }

      
      // Check bathroom temp pin. if true we need to close the valve
      if (getTemp(bathTempPin, int(settemp))){
        // Only close the valve if the valve is already open
        if (bathOpen){
          bathOpen = controlValves(bathValvePower, bathValveDir, CLOSE);
          bathMoving = true;
        }
      }
      else{
        // Only open the valve if the valve is already closed
        if (!bathOpen){
          bathOpen = controlValves(bathValvePower, bathValveDir, OPEN);
          bathMoving = true;
        }
      }

      
      // Check kitchen temp pin. if true we need to close the valve
      if (getTemp(kitchenTempPin, int(settemp))){
        // Only close the valve if the valve is already open
        if (kitchenOpen){
          kitchenOpen = controlValves(kitchenValvePower, kitchenValveDir, CLOSE);
          kitchenMoving = true;
        }
      }
      else{
        // Only open the valve if the valve is already closed
        if (!kitchenOpen){
          kitchenOpen = controlValves(kitchenValvePower, kitchenValveDir, OPEN);
          kitchenMoving = true;
        }
      }
      
    }
    // The heater is not running, assume valve positions for AC
    else if (heaterstatenum == 0){
      // Close the bedroom bypass valve
      if (bedOpen){
        bedOpen = controlValves(bedValvePower, bedValveDir, CLOSE);
        bedMoving = true;
      }
      // Open the kitchen valve
      if (!kitchenOpen){
        kitchenOpen = controlValves(kitchenValvePower, kitchenValveDir, OPEN);
        kitchenMoving = true;
      }
      // Close the bathroom valve
      if (bathOpen){
        bathOpen = controlValves(bathValvePower, bathValveDir, CLOSE);
        bathMoving = true;
      } 
      if (!fanOpen){
        fanOpen = controlValves(fanValvePower, fanValveDir, OPEN);
        fanMoving = true;
      }
    }
    
    
    
    if (controlenable == "True") {
      if (int(settemp) > int(currtemp) && (digitalRead(inhibswitch) == LOW) && (heatererror <= 1))  {
        Serial.println("Temperature Below Lower Limit. Turning On");
        if (heaterstatenum == 0) {
          uint8_t data1[24] = {0x78,0x16,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x32,0x08,0x23,0x05,0x00,0x01,0x2C,0x0D,0xAC,0x8D,0x82};
          delay(50);
          sOne.write(data1, 24);
          // Dont check if the fan valve is moving. Regardless we need to send a close signal ( if the program starts up thinking the valve is closed but it really isnt we can melt the fan )
          fanOpen = controlValves(fanValvePower, fanValveDir, CLOSE);
          fanMoving = true;
         }
      }
      
      if (int(settemp) < (int(currtemp) - 1) && (digitalRead(inhibswitch) == LOW)) {
        Serial.println("Temperature Above Upper Limit. Turning Off");
        if (heaterstatenum == 5) {
          uint8_t data1[24] = {0x78,0x16,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x32,0x08,0x23,0x05,0x00,0x01,0x2C,0x0D,0xAC,0x61,0xD6};
          delay(50);
          sOne.write(data1, 24);
          }
        }
      }
    }

    incrementTimers();
  } // loop
