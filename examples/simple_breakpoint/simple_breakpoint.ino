#include "TeensyDebug.h"

#pragma GCC optimize ("O0")

volatile int mark = 0;

void break_me() {
  Serial.print("BREAK at 0x");
  Serial.println(debug.getRegister("pc"), HEX);
}

//
// Either set debugging to O0 or mark the function
// as DEBUGRUN. If you don't do this, the optimizer will
// inline the function and you won't be able to set a
// breakpoint.
//

DEBUGRUN
void test_function() {
  mark++;
}

void setup() {
  delay(3000);
  debug.begin();
  debug.setCallback(break_me);
  debug.setBreakpoint(test_function);
}

void loop() {
  test_function();
  Serial.print("mark=");Serial.println(mark);
  delay(1000);
}