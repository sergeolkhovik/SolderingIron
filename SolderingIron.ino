#include <SevSeg.h>

SevSeg sevseg;

byte digits      = 3;
byte digitPins[] = { 11, 12, 13 };
byte segPins[]   = { 2, 4, 6, 8, 9, 3, 5, 7 };
char dash[]      = { "---" };
char hotMsg[]    = { "HOT" };
char errMsg[]    = { "ERR" };

#define E_OK  0
#define E_ALL 1
#define E_HOT 2

#define ENCL 0  // A0, input from left PIN
#define ENCR 1
#define ENCB 2  // A2, encoder button
#define TEMP 3  // current temperature
#define HOTT 7
#define OUTT 10 // temperature control

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
#define HOTL_ERR 900

#define CUR_TEMP_DELAY 1000
#define SET_TEMP_DELAY 5000

int encL = 1, encR = 1, encB = 1, lastEncB = 1;
unsigned long encBTimer = 0;

unsigned long setTempTimer = 0;

int setTemp = MIN_TEMP;
// 0 - nothing
// 1 - preparing
// 2 - left
// 3 - right
int modeSetTemp = 0;

int curTemp = 0, dispTemp = 0, curOutTemp = 0;
unsigned long curTempTimer = 0;
int tempHist[TEMP_HIST];

int error = 0;

int i;

void setup() {
  Serial.begin(250000);
   
  sevseg.begin(COMMON_ANODE, digits, digitPins, segPins);
  sevseg.setBrightness(50);

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
//  encB = analogRead(ENCB);
//  encB = encB > 1000 ? 1 : 0;

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
    Serial.print("v = "); Serial.print(v); Serial.print(", t = "); Serial.println(t);
    //if (t > MAX_TEMP) t = MAX_TEMP; // test
    //if (t < (curTemp - DIF_TEMP) || t > (curTemp + DIF_TEMP)) {
    t = map(t, 0, 1023, 0, 5000);
    t = map(t, 0, 1200, 20, 400);
//    t *= FIX_TEMP;
    // if (t > 0) {
    //  curTemp = t;
    // }

    if (t > ERR_TEMP) {
      error = E_ALL;
    } else {
Serial.print("he = "); Serial.println(h);
      dispTemp = curTemp = t;
      for (i = 0; i < TEMP_HIST - 1; i++) {
        tempHist[i] = tempHist[i+1];
        dispTemp += tempHist[i];
      }
      tempHist[TEMP_HIST-1] = curTemp;
      dispTemp /= TEMP_HIST;
  
      Serial.print("T = "); Serial.print(curTemp); Serial.print(" / "); Serial.println(dispTemp);
      curTempTimer = millis();
    }
  }

  if (error != E_OK) {
    Serial.print("he = "); Serial.println(h);
    switch (error) {
      case E_HOT:
        sevseg.setChars(hotMsg);
        break;
      default:
        sevseg.setChars(errMsg);
        break;
    }
    sevseg.refreshDisplay();
    lastEncB = 1;
    analogWrite(OUTT, 0);
    return;
  }
  
  if (encB == 0 && encBTimer == 0) {
    encBTimer = millis();
  }
  if (encB == 1 && encBTimer > 0 && millis() > (encBTimer + ENCB_DELAY)) {
    encBTimer = 0;
    lastEncB = lastEncB ? 0 : 1;
    if (lastEncB == 0) {
      for (i = 0; i < TEMP_HIST; i++) {
        tempHist[i] = curTemp;
      }    
      dispTemp = curTemp;
    }
  }

  if (lastEncB == 0) {
    switch (modeSetTemp) {
      case 0:
        if (encL == 0 && encR == 0) {
          modeSetTemp = 1;
        }
        break;
      case 1:
        if (encL == 1 && encR == 0) {
          modeSetTemp = 2;
        } else if (encL == 0 && encR == 1) {
          modeSetTemp = 3;
        }
        break;
      case 2:
        if (encR == 1) {
          setTemp += SHF_TEMP;
          modeSetTemp = 0;
          setTempTimer = millis();
        }
        break;
      case 3:
        if (encL == 1) {
          setTemp -= SHF_TEMP;
          modeSetTemp = 0;
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
  if (lastEncB == 0 && curTemp < setTemp) {
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
//    Serial.print("lastEncB = "); Serial.print(lastEncB); Serial.print(", ");
//    Serial.print("curTemp = "); Serial.print(curTemp); Serial.print(", ");
//    Serial.print("setTemp = "); Serial.print(setTemp); Serial.print(", ");
//    Serial.print("outTemp = "); Serial.print(outTemp); Serial.println("");
  }
}
