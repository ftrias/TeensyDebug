Live debugging on Teensy & GDB support
===========================================

This module provides breakpoint support for the Teensy 3/4 platform from PJRC. The module provides two features:

1. Ability to set/clear breakpoints and query registers from within Teensy code.

2. GDB Remote Serial Protocol stub so that GDB can connect to the Teensy and perform live debugging.

For background see: https://forum.pjrc.com/threads/26358-Software-Debugger-Stack

Stand-alone usage
-------------------------------------------

```C
#include "TeensyDebug.h"

#pragma GCC optimize ("O0")

int mark1 = 0;

void break_me() {
  Serial.print("BREAK at 0x"); Serial.println(debug.getRegister("pc"), HEX);
}

void test_function() {
  mark1++;
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
```

First, you must disable optimizations. If not, GCC will inline `test_function()` and you won't be able to set a breakpoint on it. For every breakpoint, `break_me` will be called. If you don't specify a callback, the default is just to print the registers and keep going. Note that the Teensy will not halt on the breakpoint. If you want to stop or delay, you must add that code yourself with a `delay()` or similar.

GDB usage
-------------------------------------------

GDB Remote Serial Protocol stub provides a simple interface between GDB and a remote target via a serial interface, such as a socket, pipe, UART, USB, etc. It works like this:

```
[PC running GDB] <--(serial)--> [Teensy GDB stub]
```

Teensduino comes with a GDB executable for ARM processors.

To start, I recommend you configure Teensy for Dual Serial support. You could compile with Dual serial ports using the menu `Tools / USB Type`. After uploading, you can figure out the serial device name using the menu `Tools / Port`. Alternatively you can just use a physical serial port.

```C
#include "TeensyDebug.h"

#pragma GCC optimize ("O0")

int mark1 = 0;

void test_function() {
  mark1++;
}

void setup() {
  delay(3000);
  debug.begin(SerialUSB1);
}

void loop() {
  test_function();
  Serial.print("mark=");Serial.println(mark);
  delay(1000);
}
```

After compiling and uploading this program, Teensy will have two serial ports. One is the standard one you can view on the Serial Monitor. The other is the one you will connect to. You need to figure out what the device name is (See menu `Tools / Port`). Let's assume it's `/dev/cu.usbmodem61684901`.

You also need to find the GDB executable that came with Teensyduino. On the Mac it is located in `/Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb`.

Running GDB yields:

```
$ /Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb
GNU gdb (GNU Tools for ARM Embedded Processors) 7.10.1.20160923-cvs
Copyright (C) 2015 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.  Type "show copying"
and "show warranty" for details.
This GDB was configured as "--host=x86_64-apple-darwin10 --target=arm-none-eabi".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
<http://www.gnu.org/software/gdb/documentation/>.
For help, type "help".
Type "apropos word" to search for commands related to "word".
(gdb)
```

At the prompt type the `target remote` command using the correct port:

```
target remote /dev/cu.usbmodem61684903
```

This will connect and you should be able to use normal GDB commands and symbols.

```
(gdb) p mark
$1 = 5
(gdb) b test_function
Breakpoint 1 at 0xb0: file /Users/ftrias/Documents/Source/TeensyDebug/examples/breakpoint_test/breakpoint_test.ino, line 15.
(gdb) c
Continuing.

Program received signal SIGTRAP, Trace/breakpoint trap.
0x000000b2 in test_function () at /Users/ftrias/Documents/Source/TeensyDebug/examples/breakpoint_test/breakpoint_test.ino:15
15	void test_function() {
(gdb) 
```

Internal workings
---------------------------------------------

This is how it works with Teensy.

1. By using a timer, the Teensy listens to GDB commands from a serial device.

2. When it gets commands like memory queries, memory sets and things that don't require halting, it responds with the data requested.

3. When it receives a halt command, Teensy will just go into a loop querying for commands and responding. It won't return to it's caller until GDB tells it to do so. Thus, execution of the main thread will stop but interrupts will continue. Because interrupts continue, on the plus side, the Teensy won't die and USB and other features will stay active. On the other hand, sometimes you just the want the system to halt. Perhaps there could be an option to halt all interrupts as well or change the priority. Keeping interrupts going is probably easier for beginners and models what desktop apps do (when an apps stops, the OS keeps going).

4. Provide a special hardwired "breakpoint" instruction that you can insert into your code. Each breakpoint will have a flag in RAM to determine if it is enabled or not. If enabled, when execution reaches it, it will execute an interrupt (software or SVC). If disabled, execution just keeps going. Breakpoints are enabled/disabled based on commands received from GDB.

5. If a function is placed in RAM, you can dynamically insert/remove SVC calls in the code, in the same way that standard debuggers work. Teensy 4 places all user code in RAM. On Teensy 3, you can put a function by specifying FASTRAM. Again, breakpoints like this can be set and enabled/disabled by GDB.

6. On the Teensy 3.2, we could as well use the Flash Patch Block to set and remove SVC calls using patching. Thus, you can dynamically set breakpoints in flash. Teensy 4 doesn't support this, but since it places code in RAM, that's probably not a big deal.

Future considerations
---------------------------------------------

The serial connection can be anything that supports reading and writing ASCII in sequence. To start it's probably best to use a UART or USB Serial but in theory it could be USB Serial, CAN, network, Raw connection, etc.

Right now, running GDB will have to be done manually. But in the future, GDB could be piped to Arduino's serial monitor. Both GDB's output and Teensy's serial output could be sent to the display. GDB can receive commands from the Send window. If, in addition to this, we use USB for the serial connection to GDB, then Teensy will have onboard live debugging available with no special setup or hardware required.

```
[Arduino]      [           ser1] <-- [Teensy & GDB stub]
[Serial ] <--> [gdb proxy      ]        
[Monitor]      [           ser2] <--> [GDB]
```
