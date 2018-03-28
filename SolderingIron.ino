#include <SevSeg.h>

SevSeg sevseg;

byte digits      = 3;
byte digitPins[] = { 11, 12, 13 };
byte segPins[]   = { 2, 4, 6, 8, 9, 3, 5, 7 };
char dash[]      = { "---" };
char hotMsg[]    = { "HOT" };
char errMsg[]    = { "ERR" };

#define DEBUG 1

#define E_OK  0
#define E_ALL 1
#define E_HOT 2

#define ENCL 0  // A0, input from left PIN
#define ENCR 1
#define ENCB 2  // A2, encoder button
#define TEMP 3  // current temperature
#define HOTT 7
#define OUTT 10 // temperature control

#define TEMP_A 250
#define TEMP_B 350
#define TEMP_BUTTON_A 4 // predefined temperature, button A
#define TEMP_BUTTON_B 5

#define ENCT 800 // encoder value threshold
#define ENCB_DELAY 500

#define MIN_TEMP 180 // min allowed temperature
#define MAX_TEMP 480 // max allowed temperature
#define SHF_TEMP   5 //
#define DIF_TEMP   3 // ignore temp deviation
#define ERR_TEMP 600
#define TEMP_HIST 20

#define HOTL_DELAY 5000
#define HOTL_HOT 390
#define HOTL_ERR 520

#define CUR_TEMP_DELAY 1000
#define SET_TEMP_DELAY 5000

int encL = 1, encR = 1, encB = 1, offMode = 1;
unsigned long encBTimer = 0;
unsigned long tempATimer = 0;
unsigned long tempBTimer = 0;

unsigned long setTempTimer = 0;

int setTemp = MIN_TEMP;
// 0 - nothing
// 1 - preparing
// 2 - left
// 3 - right
int setTempMode = 0;

int curTemp = 0, dispTemp = 0, curOutTemp = 0;
unsigned long curTempTimer = 0;
int tempHist[TEMP_HIST];

int error = 0;

int i;

void setup() {
  Serial.begin(250000);
   
  sevseg.begin(COMMON_ANODE, digits, digitPins, segPins);
  sevseg.setBrightness(100);

  pinMode(OUTT, OUTPUT);
  analogWrite(OUTT, 0);

  for (i = 0; i < TEMP_HIST; i++) {
    tempHist[i] = 0;
  }
}

int getEncValue(int pin) {
  int val = analogRead(pin);
  return val > ENCT ? 1 : 0;
}

void loop() {
  encL = getEncValue(ENCL);
  encR = getEncValue(ENCR);
  encB = getEncValue(ENCB); // 1000?

  int h = analogRead(HOTT);
  if (h < HOTL_HOT) {
    error = E_HOT;
  } else if (h > HOTL_ERR) {
    error = E_ALL;
  } else {
    error = E_OK;
  }

  if (curTempTimer > 0 && (curTempTimer + CUR_TEMP_DELAY) < millis()) {
    curTempTimer = 0;
  }

  if (error == E_OK && (curTemp == 0 || curTempTimer == 0)) {
    int t = analogRead(3);
    float v = t * 4.9;
    if (DEBUG) {
      Serial.print("v = "); Serial.print(v); Serial.print(", t = "); Serial.println(t);
    }
    //if (t > MAX_TEMP) t = MAX_TEMP; // test
    //if (t < (curTemp - DIF_TEMP) || t > (curTemp + DIF_TEMP)) {
    t = map(t, 0, 1023, 0, 5000);
    t = map(t, 0, 3600, 20, 400); // original value - 1800
//    t *= FIX_TEMP;
    // if (t > 0) {
    //  curTemp = t;
    // }

    if (t > ERR_TEMP) {
      error = E_ALL;
    } else {
//      if (DEBUG) {
//        Serial.print("he = "); Serial.println(h);
//      }
      dispTemp = curTemp = t;
      for (i = 0; i < TEMP_HIST - 1; i++) {
        tempHist[i] = tempHist[i+1];
        dispTemp += tempHist[i];
      }
      tempHist[TEMP_HIST-1] = curTemp;
      dispTemp /= TEMP_HIST;

      if (DEBUG) {
        Serial.print("T = "); Serial.print(curTemp); Serial.print(" / "); Serial.println(dispTemp);
      }
      curTempTimer = millis();
    }
  }

  if (error != E_OK) {
    if (DEBUG) {
      Serial.print("he = "); Serial.println(h);
    }
    switch (error) {
      case E_HOT:
        sevseg.setChars(hotMsg);
        break;
      default:
        sevseg.setChars(errMsg);
        break;
    }
    sevseg.refreshDisplay();
    offMode = 1;
    analogWrite(OUTT, 0);
    return;
  }
  
  if (encB == 0 && encBTimer == 0) {
    encBTimer = millis();
  }
  if (encB == 1 && encBTimer > 0 && millis() > (encBTimer + ENCB_DELAY)) {
    encBTimer = 0;
    offMode = offMode ? 0 : 1;
    if (offMode == 0) {
      for (i = 0; i < TEMP_HIST; i++) { //  fill history buffer with current temperature
        tempHist[i] = curTemp;
      }    
      dispTemp = curTemp;
    }
  }

  if (encB == 1) {
    int b = getEncValue(TEMP_BUTTON_A);
    if (b == 0) {
      tempATimer = millis();
    }
    if (b == 1 && tempATimer > 0 && millis() > (tempATimer + ENCB_DELAY)) {
      tempATimer = 0;
      setTemp = TEMP_A;
      setTempTimer = millis();
      offMode = 0;
    }
    b = getEncValue(TEMP_BUTTON_B);
    if (b == 0) {
      tempBTimer = millis();
    }
    if (b == 1 && tempBTimer > 0 && millis() > (tempBTimer + ENCB_DELAY)) {
      tempBTimer = 0;
      setTemp = TEMP_B;
      setTempTimer = millis();
      offMode = 0;
    }
  }

  if (offMode == 0) {
    switch (setTempMode) {
      case 0:
        if (encL == 0 && encR == 0) {
          setTempMode = 1;
        }
        break;
      case 1:
        if (encL == 1 && encR == 0) {
          setTempMode = 2;
        } else if (encL == 0 && encR == 1) {
          setTempMode = 3;
        }
        break;
      case 2:
        if (encR == 1) {
          setTemp += SHF_TEMP;
          setTempMode = 0;
          setTempTimer = millis();
        }
        break;
      case 3:
        if (encL == 1) {
          setTemp -= SHF_TEMP;
          setTempMode = 0;
          setTempTimer = millis();
        }
        break;
    }
  
    if (setTemp < MIN_TEMP) setTemp = MIN_TEMP;
    if (setTemp > MAX_TEMP) setTemp = MAX_TEMP;

    if (setTempTimer > 0 && millis() > (setTempTimer + SET_TEMP_DELAY)) {
      setTempTimer = 0;
    }

    if (setTempTimer > 0) {
      sevseg.setNumber(setTemp);
    } else {
      sevseg.setNumber(dispTemp);
    }
  } else {
    sevseg.setChars(dash);
  }
  sevseg.refreshDisplay();

  int outTemp = 0;
  if (offMode == 0 && curTemp < setTemp) {
    int d = setTemp - curTemp;
    if (d > 16) {
      outTemp = 230;
    } else if (d > 6) {
      outTemp = 99;
    } else if (d > 4) {
      outTemp = 80;
    } else {
      outTemp = 45;
    }
  }
  if (outTemp != curOutTemp) {
    analogWrite(OUTT, outTemp);
    curOutTemp = outTemp;
  }
}
