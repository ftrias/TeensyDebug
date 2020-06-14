#include "TeensyDebug.h"

//
// Either set debugging to O0 or mark the function
// as DEBUGRUN. If you don't do this, the optimizer will
// inline the function and you won't be able to set a
// breakpoint.
//
#pragma GCC optimize ("O0")

volatile int mark = 0;

void test_function() {
  mark++;
}

void setup() {
}

void loop() {
  test_function();
  Serial.print("mark=");Serial.println(mark);
  delay(1000);
}