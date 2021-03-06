#define VERSION     "\n\nAndroTest V2.0 - @kas2014\ndemo for V5.X App (6 button version)"

#include "SoftwareSerial.h"

signed int x =0; 
signed int y =0;
signed int old_x=0; 
signed int old_y=0;
signed int BT_x =0;
signed int BT_y =0;
signed int AutoPilot_x=0; 
signed int AutoPilot_y=0;
float Current =0;
// float CurentOLD = 0;

float  dist_cm =0;
float  dist_cm2 =0;
long SonarInterval=300;
long RotateInterval = 500;
long FaleSafeInterval = 2000;
int  AutopilotRotate=1;
long RotateIntervalRND = 0;
long RazgonTimeOut =100;
long CurentMeterInt = 300;
int  SonaeEN = 1;
int  AutoPilotEN = 0;
int  MotorOK = 0;

#define RAZGON_STEP 7
#define CURENT_PORT A7
#define SLOWCOUNT 10
#define FASTCOUNT 32

// в сантиметрах (distance threshold) Пороги расстояний до препятствия
// Если ближе, то резкий разворот на месте, иначе плавный доворот
const int DST_TRH_TURN = 30;   //40
// Если ближе, то стоп и назад
const int DST_TRH_BACK = 15;       //25

 // Виды поворотов
const byte MOTOR_ROTATE_RIGHT = 0;  // вправо резкий разворот на месте (все левые колеса крутятся вперед, все правые - назад)
const byte MOTOR_TURN_RIGHT   = 1;  // вправо плавный поворот
const byte MOTOR_ROTATE_LEFT  = 2;  // влево резкий разворот на месте
const byte MOTOR_TURN_LEFT    = 3;  // влево плавный поворот
const byte MOTOR_TURN_BACK_RIGHT = 4; // поворот вправо задним ходом
const byte MOTOR_TURN_BACK_LEFT  = 5;
const byte MOTOR_FORWARD = 6;         // Движение вперед

byte MOTOR_PREV_DIRECTION; // предыдущее выполненное направление движения



int ML1 = 5;
int ML2 = 4;
int MR1 = 13;
int MR2 = 12;

int EL = 3;   
int ER = 11; 

int el = 0;
int er = 0;

int elp = 0;
int erp = 0;

#define    STX          0x02
#define    ETX          0x03
#define    ledPin       13
#define    SLOW         750                            // Datafields refresh rate (ms)
#define    FAST         250                             // Datafields refresh rate (ms)

#define   RANDOMPORT 5
#define   IR_SENSOR  10

#include "Ultrasonic.h"

// sensor connected to:
// Trig - 12, Echo - 13
Ultrasonic ultrasonic1(9, 8);
Ultrasonic ultrasonic2(15, 14);


SoftwareSerial mySerial(7,6);                           // BlueTooth module: pin#2=TX pin#3=RX
byte cmd[8] = {0, 0, 0, 0, 0, 0, 0, 0};                 // bytes received
byte buttonStatus = 0;                                  // first Byte sent to Android device
long previousMillis = 0;                                // will store last time Buttons status was updated
long sendInterval = SLOW;                               // interval between Buttons status transmission (milliseconds)
String displayStatus = "ROBOTANK";                          // message to Android device

void setup()  {
    Serial.begin(57600);
    mySerial.begin(57600);                                // 57600 = max value for softserial
    pinMode(ledPin, OUTPUT);     
    Serial.println(VERSION);
    while(mySerial.available())  mySerial.read();         // empty RX buffer
    randomSeed(analogRead(RANDOMPORT));
  
    pinMode(ER, OUTPUT); 
    pinMode(EL, OUTPUT); 
    pinMode(ML1, OUTPUT); 
    pinMode(ML2, OUTPUT); 
    pinMode(MR1, OUTPUT); 
    pinMode(MR2, OUTPUT); 
    
    pinMode(11, OUTPUT); 
    //pinMode(A0, OUTPUT); 
    //pinMode(A1, OUTPUT); 
    pinMode(IR_SENSOR,INPUT);
    digitalWrite(2,LOW);
}

void loop() {
    BTjoystic();
    SonarDistance();
    CurrentMeter();
    Autopilot();
    Mixer();
    process();
    sendBlueToothData();
}

void BTjoystic(){
  static long previousMillis = 0;                             
  long currentMillis = millis();
     
  if(currentMillis - previousMillis > FaleSafeInterval) x=0; y=0; // если сигнал с ьлуетуза пропал то останавливаем движки
       
  if(mySerial.available())  {                           // data received from smartphone
    delay(2);
    previousMillis = currentMillis;    
    cmd[0] =  mySerial.read();  
    if(cmd[0] == STX)  {
      int i=1;      
      while(mySerial.available())  {
        delay(1);
        cmd[i] = mySerial.read();
        if(cmd[i]>127 || i>7)                 break;     // Communication error
        if((cmd[i]==ETX) && (i==2 || i==7))   break;     // Button or Joystick data
        i++;
      }
      if     (i==2)          getButtonState(cmd[1]);    // 3 Bytes  ex: < STX "C" ETX >
      else if(i==7)          getJoystickState(cmd);     // 6 Bytes  ex: < STX "200" "180" ETX >
    }
  } 
}

void sendBlueToothData()  {
    static long previousMillis = 0;                             
    long currentMillis = millis();
    if(currentMillis - previousMillis > sendInterval) {   // send data back to smartphone
      previousMillis = currentMillis; 
  
  // Data frame transmitted back from Arduino to Android device:
  // < 0X02   Buttons state   0X01   DataField#1   0x04   DataField#2   0x05   DataField#3    0x03 >  
  // < 0X02      "01011"      0X01     "120.00"    0x04     "-4500"     0x05  "Motor enabled" 0x03 >    // example
  
      mySerial.print((char)STX);                                             // Start of Transmission
      mySerial.print(getButtonStatusString());  mySerial.print((char)0x1);   // buttons status feedback
      //mySerial.print(GetdataInt1());            mySerial.print((char)0x4);   // datafield #1
      mySerial.print(dist_cm2);                  mySerial.print((char)0x4);   // datafield #1
      mySerial.print(Current);                   mySerial.print((char)0x5);   // datafield #2
      mySerial.print(displayStatus);                                         // datafield #3
      mySerial.print((char)ETX);                                             // End of Transmission
  }  
}

String getButtonStatusString()  {
    String bStatus = "";
    for(int i=0; i<6; i++)  {
      if(buttonStatus & (B100000 >>i))      bStatus += "1";
      else                                  bStatus += "0";
    }
    return bStatus;
}

int GetdataInt1()  {              // Data dummy values sent to Android device for demo purpose
    static int i= -30;              // Replace with your own code
    i ++;
    if(i >0)    i = -30;
    return i;  
}

float GetdataFloat2()  {           // Data dummy values sent to Android device for demo purpose
    static float i=50;               // Replace with your own code
    i-=.5;
    if(i <-50)    i = 50;
    return i;  
}

void getJoystickState(byte data[8])    {
    int joyX = (data[1]-48)*100 + (data[2]-48)*10 + (data[3]-48);       // obtain the Int from the ASCII representation
    int joyY = (data[4]-48)*100 + (data[5]-48)*10 + (data[6]-48);
    joyX = joyX - 200;                                                  // Offset to avoid
    joyY = joyY - 200;                                                  // transmitting negative numbers
  
    if(joyX<-100 || joyX>100 || joyY<-100 || joyY>100)     return;      // commmunication error
    
// Your code here ...
    Serial.print("Joystick position:  ");
    Serial.print(joyX);  
    Serial.print(", ");  
    Serial.println(joyY); 

    BT_x = joyX;
    BT_y = joyY;
   // process();
}

void getButtonState(int bStatus)  {
  switch (bStatus) {
// -----------------  BUTTON #1  -----------------------
    case 'A':
      buttonStatus |= B000001;        // ON
      Serial.println("\n** Button_1: ON **");
      // your code...      
      displayStatus = "Sunar <ON>";
      Serial.println(displayStatus);
      SonaeEN = 1;
      break;
    case 'B':
      buttonStatus &= B111110;        // OFF
      Serial.println("\n** Button_1: OFF **");
      // your code...      
      displayStatus = "Sonar <OFF>";
      Serial.println(displayStatus);
      SonaeEN =0;
      break;

// -----------------  BUTTON #2  -----------------------
    case 'C':
      buttonStatus |= B000010;        // ON
      Serial.println("\n** AutoPilot: ON **");
      // your code...      
      displayStatus = "AutoPilot <ON>";
      AutoPilotEN =1;
      break;
    case 'D':
      buttonStatus &= B111101;        // OFF
      Serial.println("\n** AutoPilot: OFF **");
      // your code...      
      displayStatus = "AutoPilot <OFF>";
      AutoPilotEN =0;
      break;

// -----------------  BUTTON #3  -----------------------
    case 'E':
      buttonStatus |= B000100;        // ON
      Serial.println("\n** Svet ON **");
      // your code...      
      digitalWrite(2,HIGH);
      displayStatus = "Swet ON"; // Demo text message
      Serial.println(displayStatus);
      break;
    case 'F':
      buttonStatus &= B111011;      // OFF
      Serial.println("\n** Svet OFF **");
      // your code...      
      digitalWrite(2,LOW);
      displayStatus = "Svet OFF";
      Serial.println(displayStatus);
      break;

// -----------------  BUTTON #4  -----------------------
    case 'G':
      buttonStatus |= B001000;       // ON
      Serial.println("\n** Button_4: ON **");
      // your code...      
      displayStatus = "Datafield update <FAST>";
      Serial.println(displayStatus);
      sendInterval = FAST;
      break;
    case 'H':
      buttonStatus &= B110111;    // OFF
      Serial.println("\n** Button_4: OFF **");
      // your code...      
      displayStatus = "Datafield update <SLOW>";
      Serial.println(displayStatus);
      sendInterval = SLOW;
     break;

// -----------------  BUTTON #5  -----------------------
    case 'I':           // configured as momentary button
//      buttonStatus |= B010000;        // ON
      Serial.println("\n** Button_5: ++ pushed ++ **");
      // your code...      
      displayStatus = "Button5: <pushed>";
      break;
//   case 'J':
//     buttonStatus &= B101111;        // OFF
//     // your code...      
//     break;

// -----------------  BUTTON #6  -----------------------
    case 'K':
      buttonStatus |= B100000;        // ON
      Serial.println("\n** Button_6: ON **");
      // your code...      
       displayStatus = "Button6 <ON>"; // Demo text message
     break;
    case 'L':
      buttonStatus &= B011111;        // OFF
      Serial.println("\n** Button_6: OFF **");
      // your code...      
      displayStatus = "Button6 <OFF>";
      break;
  }
// ---------------------------------------------------------------
}

/*
void CurrentMeter()
{
  static long previousMillis = 0;                             
  long currentMillis = millis();
  
  if(currentMillis - previousMillis > CurentMeterInt) 
   {   // send data back to smartphone
      previousMillis = currentMillis; 
//      CurrentOLD = Current;
      Current = 0.0264 * (analogRead(CURENT_PORT) -512);
      //Curent = Curent + (0.0264 * analogRead(CURENT_PORT) -13.51) / 1000;
      Serial.println("Curent A ");   
      Serial.println(Current);   
   }
}

*/
long GetInstantCurrent()
{
 long value = 0;
 for (int i = 0; i < FASTCOUNT; ++i)
 value += analogRead(CURENT_PORT);
 value /= FASTCOUNT;
 return value;
}

inline float GetCurrentFromLong(long value)
{
 //Serial.println("Curent ADC ");   
 //Serial.println(value);   
 return  0.0264 * (value - 507);  //0.0264 0.0376
}


long CountBuf[SLOWCOUNT] = { 0 };
int curCount = 0;
long SummCount = 0;

long GetIntegratedCurrent()
{
 SummCount -= CountBuf[curCount];
 CountBuf[curCount] = GetInstantCurrent();
 SummCount += CountBuf[curCount];
 if (++curCount >= SLOWCOUNT)
  curCount = 0;
 return (SummCount / SLOWCOUNT);
}
 
void CurrentMeter()
{
 Current = - GetCurrentFromLong(GetIntegratedCurrent());
// Serial.println("Curent A ");   
// Serial.println(Current);   
}


void SonarDistance()
{
  static long previousMillis = 0;                             
  long currentMillis = millis();
  
  if(currentMillis - previousMillis > SonarInterval) 
   {   // send data back to smartphone
      previousMillis = currentMillis; 
      dist_cm = ultrasonic1.distanceRead();              //Ranging(CM); get distance
      Serial.println(dist_cm);                      // print the distance
      dist_cm2 = ultrasonic2.distanceRead();        //Ranging(CM);       get distance
      Serial.println(dist_cm2);                      // print the distance
   
   }
}

//void Autopilot2

void Autopilot()
{
  if(AutoPilotEN == 1)
  {
    int rnd =0 ;
    static long previousMillis = 0;                             
    long currentMillis = millis();
     
    if(currentMillis - previousMillis > RotateInterval) 
     {// send data back to smartphone
      previousMillis = currentMillis;    
      RotateIntervalRND = RotateInterval + 100 * random(1,4);      //4

      Serial.println("AUTOPILOT WORK");

      // определить направление поворота
      // прямо
      //int Ir = digitalRead(pushButton);
      if(AutopilotRotate==15) 
       {
          AutopilotRotate = 0;
          rnd = random(1, 10);
          if (rnd > 5)MotorTurnBackRight();
          else MotorTurnBackLeft();        
            return; 
       }

      AutopilotRotate++;
      
      if (( dist_cm < DST_TRH_TURN && dist_cm >DST_TRH_BACK) || (dist_cm2 < DST_TRH_TURN && dist_cm2 >DST_TRH_BACK) || digitalRead(IR_SENSOR) == 0)   
      {
        // направление поворота выбираем рандомно
        if(MOTOR_PREV_DIRECTION == MOTOR_FORWARD)
        {
          //rnd = random(1, 10);
          //if (rnd > 5) MotorTurnRotateRight();
          if (dist_cm >= dist_cm2) MotorTurnRotateRight();
          else MotorTurnRotateLeft();
          
        } else if(MOTOR_PREV_DIRECTION == MOTOR_ROTATE_RIGHT) MotorTurnRotateRight(); //поворачиваем в туже сторону пока не кончится препядствие
               else MotorTurnRotateLeft();
                
      }
      else if ( dist_cm <= DST_TRH_BACK || dist_cm2 <= DST_TRH_BACK )
      {
            // ранее уже поворачивали задним ходом влево?
            if (MOTOR_TURN_BACK_LEFT == MOTOR_PREV_DIRECTION) MotorTurnBackRight();
            else MotorTurnBackLeft();        
            return; 
            
      }
      else MotorForward();
    }
  }
  else
  {
     if(BT_y>0 && dist_cm < 30) BT_y=25;
     if(SonaeEN == 1) if(BT_y>0 && dist_cm < 10) BT_y=0;  
  }
}


void MotorControl(signed int MotorX, signed int MotorY, long MotorWorkInt)
{
    static long previousMillis = 0;                             
    long currentMillis = millis();
         
    if(currentMillis - previousMillis > MotorWorkInt) 
     {
        previousMillis = currentMillis;    
        AutoPilot_x = MotorX;
        AutoPilot_y = MotorY;
        MotorOK = 1;
     } else MotorOK = 0;
}

void MotorForward()
{
    MotorControl(0, 45, RotateIntervalRND);
    if(MotorOK ==1)
    {
        MOTOR_PREV_DIRECTION = MOTOR_FORWARD;
    }
    Serial.println("EDEM PRAMO");
}

void MotorTurnBackLeft()
{
    MotorControl(-35, -40, RotateIntervalRND);
    if(MotorOK ==1)
    {
      MOTOR_PREV_DIRECTION = MOTOR_TURN_BACK_LEFT;
      //AutopilotRotate=1;  
    }
}

void MotorTurnBackRight()
{
    MotorControl(35, -40, RotateIntervalRND);
    if(MotorOK ==1)
    {
      MOTOR_PREV_DIRECTION = MOTOR_TURN_BACK_RIGHT;
      //AutopilotRotate=1;  
    }
}

void MotorTurnRotateLeft()
{
     MotorControl(-60, 0, RotateIntervalRND);
     if(MotorOK ==1)
     {
        MOTOR_PREV_DIRECTION = MOTOR_ROTATE_LEFT; 
        //AutopilotRotate=1;  
     }
}

void MotorTurnRotateRight()
{
     MotorControl(60, 0, RotateIntervalRND);
     if(MotorOK ==1)
     {
        MOTOR_PREV_DIRECTION = MOTOR_ROTATE_RIGHT;  
        //AutopilotRotate=1;   
     }
}

void MotorStop()
{
    //AutoPilot_x=0;
     //AutoPilot_y=0; 
     MotorControl(0, 0, 10);
}

void Mixer()
{
  if(AutoPilotEN == 1 && BT_x ==0 && BT_y ==0)
  {
    x = AutoPilot_x;
    y = AutoPilot_y;
  }
  else
  {
    x=BT_x;
    y=BT_y;
  }
}


void process(){

    static long previousMillis = 0;                             
    long currentMillis = millis();
     
    if(currentMillis - previousMillis > RazgonTimeOut) 
     {// send data back to smartphone
      previousMillis = currentMillis;    
      if(x != old_x || y != old_y)
      {
        if(x > 0 && x-old_x > 10) x= old_x + RAZGON_STEP;    //плавный разгон
        else if(x < 0 && x-old_x < -10)  x= old_x - RAZGON_STEP;  
        
        if(y > 0 && y-old_y > 10) y= old_y + RAZGON_STEP;
        else if(y < 0 && y-old_y < -10)  y= old_y - RAZGON_STEP;  
        old_x = x;
        old_y = y;
              
        signed int gaz_x = map(x, -100, 100, -192, 192);
        signed int gaz_y = map(y, -100, 100, -254, 254);
    
        
        er = gaz_y + gaz_x;
        el = gaz_y - gaz_x;
      
        //left motor control:
        
        if (er < 0) { 
          digitalWrite(MR1, HIGH);
          digitalWrite(MR2, LOW);  
        } 
        
        else if (er >= 0) { 
          digitalWrite(MR1, LOW);
          digitalWrite(MR2, HIGH);
        } 
        
        erp = abs(er);
        if(erp > 254) erp = 254;
        
        //right motor control:
      
        if (el < 0) { 
          digitalWrite(ML1, HIGH); 
          digitalWrite(ML2, LOW); 
        } 
        
        else if (el >= 0)  {
          digitalWrite(ML1, LOW); 
          digitalWrite(ML2, HIGH); 
        } 
        
        elp = abs(el);
        if(elp > 254) elp = 254;
                                                       
        analogWrite(EL, elp);  
        analogWrite(ER, erp);  
      
        Serial.println(elp);
        Serial.println(erp);
    }
  }  
}


