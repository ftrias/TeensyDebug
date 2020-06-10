#include "TeensyDebug.h"
#include "HardwareSerial.h"

#pragma GCC optimize ("O0")

volatile int mark = 45;
volatile int mark2 = 55;

void break_me() {
  Serial.println("BREAK!");
//  Serial.println(debug.getRegister("pc"), HEX);
}

//DEBUGRUN
void testme() {
  int local1=1;
  int local2=2;
//  halt();
  mark = 5;
  mark2++;
  mark2++;
}

void level2() {
  int local1b=15;
  int local2b=25;
  testme();
}

void level1() {
  int local1a=45;
  int local2a=65;
  level2();
}

void setup() {
  debug.begin();
  delay(3000);
  Serial.begin(115200);
  Serial1.begin(9600);
  debug.begin(Serial1);
  
//  debug.setCallback(break_me);
//  debug.setBreakpoint(testme, 1);

//  *(int*)0 = 0;

//  breakpoint_enable(0);
//  breakpoint(0);
}

//extern int debugcount;

void loop() {
  level1();
//  Serial.print("act=");Serial.println(debugcount);
  Serial.print("mark=");Serial.println(mark);
  delay(2000);
}
