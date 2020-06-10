#include "TeensyDebug.h"
#include "HardwareSerial.h"

int mark1 = 0;

//
// Either set debugging to O0 or mark the function
// as DEBUGRUN. If you don't do this, the optimizer will
// inline the function and you won't be able to set a
// breakpoint.
//
// #pragma GCC optimize ("O0")

DEBUGRUN
void test_function() {
  mark1++;
}

void setup() {
  delay(3000);

#ifdef CDC2_DATA_INTERFACE
  //
  // You could compile with Dual serial ports using the menu
  // Tools / USB Type. Then you won't need a physical serial
  // After uploading, you can figure out the serial device name 
  // using the menu Tools / Port.
  // Then connect in the same way:
  //     target remote /dev/cu.usbmodel.XXXXX
  //
  debug.begin(SerialUSB1);
#else
  //
  // You can also connect to Serial1 hardware serial port. On GDB, use
  //    target remote /dev/cu.usbserial.XXXXX
  //
  debug.begin(Serial1);
#endif
}

void loop() {
  test_function();
  Serial.print("mark=");Serial.println(mark);
  delay(1000);
}