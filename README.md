Live debugging on Teensy & GDB support
===========================================

This module provides breakpoint support for the Teensy 3/4 platform from PJRC without need for an external debug interface. The module provides two features:

1. Ability to set/clear breakpoints and query registers from within Teensy code.

2. GDB Remote Serial Protocol stub so that GDB can connect to the Teensy and perform live debugging.

3. Catch hard crashes and display diagnosics.

For background see: https://forum.pjrc.com/threads/26358-Software-Debugger-Stack

GDB background
===========================================

GDB Remote Serial Protocol stubs provide a simple interface between GDB and a remote target via a serial interface, such as a socket, pipe, UART, USB, etc. It works like this:

```
[PC running GDB] <--(serial)--> [Teensy GDB stub]
```

Since Teensduino comes with a GDB executable for ARM processors, there is no need to install it. Run the installer (see below) to create a new menu in Arduino that will enable GDB.

Sample code and usage
-------------------------------------------

```C
#include "TeensyDebug.h"
#pragma GCC optimize ("O0")

int mark = 0;

void test_function() {
  mark++;
}

void setup() {
  // Use the first serial port as you usually would
  Serial.begin(19200);

  // Debugger will use second USB Serial; this line is not need if using menu option
  debug.begin(SerialUSB1);

  // debug.begin(Serial1);   // or use physical serial port

  halt();                    // stop on startup; if not, Teensy keeps running and you
                             // have to set a breakpoint or use Ctrl-C.
}

void loop() {
  test_function();
  Serial.println(mark);
  delay(1000);
}
```

The `#pragma` will eliminate optimizations. If you don't use it, the compiler will inline `test_function()` and remove the symbol. It may also eliminate `mark` after realizing it serves no purpose.

Installing on Mac
-------------------------------------------

This tool requires Python, which is installed by default on Macs. Install by running `install-mac.command` located in the disribution direction. This script creates a new menu option in Arduino and copies itself to the tools directory. It makes no changes to any source code.

Copy the entire directoy to your local Arduino directory, which is usually `~/Documents/Arduino/libraries`.

Installing on Windows
-------------------------------------------

This tool is installed by running `install-windows.bat` as Administrator. Do this by right-clicking on the file and selecting `Run as administraor`. This script creates a new menu option in Arduino and copies itself to the tools directory.

Installing on Linux
-------------------------------------------
Run `install-linux.sh` to install. It assumes your Arduino is installed in `~/arduino`. If this is not, pass the direction with the `-i=path` option.

New menu options
-------------------------------------------

The new menu provides three options: "Use Dual Serial", "Take over Serial", and "Off".

* Use Dual Serial: If you compile Dual Serial support (or at least two serials), the second USB Serial will be used to communicate with GDB. All optimizations will be turned off.

* Take over Serial: GDB will use the USB Serial to communicate with the Teensy. The library will redefine Serial so that any calls to Serial will cause GDB to print your data. All optimizations will be turned off.

* Off: GDB is not used.

Running GDB manually
-------------------------------------------

If the menu option doesn't work for you, or you are using a physical serial port, you can run GDB manually.

After compiling and uploading the program in the example above, Teensy will have two serial ports. One is the standard one you can view on the Serial Monitor. The other is the one you will connect to. You need to figure out what the device name is (See menu `Tools / Port`). Let's assume it's `/dev/cu.usbmodem61684901`.

You also need to find the GDB executable that came with Teensyduino. On the Mac it is located in `/Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb`.

Next, find the ELF file created. Arduino puts it in a temporary directory. Fortunately, it is the same directory for the duration of Arduino. If you look at the end of the compile output, you should see multiple mentions of a file ending with `.elf`. For example: `/var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_133762/breakpoint_test.ino.elf`.

Run GDB followed by the ELF file location:

```
$ /Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb /var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_133762/breakpoint_test.ino.elf
```

Running GDB outputs:

```
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

At the prompt use the `target remote` command using the correct port:

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

Special GDB commands
-------------------------------------------

GDB provides a command called `monitor` for sending arbitrary text to the Teensy. Right now, this is used to implement pin IO operation, but could easily be expanded. For example, `monitor digitalWrite(13,1)` will flip pin 13 high, which turns on the LED.

These are the commands implemented so far:

* digitalRead(pin) -> returns 1 or 0
* digitalWrite(pin, 1_or_0)
* analogRead(pin)  -> returns analog input from pin
* analogWrite(pin, value)
* restart          -> reboot Teensy

Internal workings
===========================================

This is how it works:

1. By using a timer, the Teensy listens to GDB commands from a serial device.

2. When it gets commands like memory queries, memory sets and things that don't require halting, it responds with the data requested.

3. When it receives a halt command, Teensy will just go into a loop querying for commands and responding. It won't return to it's caller until GDB tells it to do so. Thus, execution of the main thread will stop but interrupts will continue. Because interrupts continue, on the plus side, the Teensy won't die and USB and other features will stay active. On the other hand, sometimes you just the want the system to halt. Perhaps there could be an option to halt all interrupts as well or change the priority. Keeping interrupts going is probably easier for beginners and models what desktop apps do (when an apps stops, the OS keeps going).

4. Provide a special hardwired "breakpoint" instruction that you can insert into your code. Each breakpoint will have a flag in RAM to determine if it is enabled or not. If enabled, when execution reaches it, it will execute an interrupt (software or SVC). If disabled, execution just keeps going. Breakpoints are enabled/disabled based on commands received from GDB.

5. If a function is placed in RAM, you can dynamically insert/remove SVC calls in the code, in the same way that standard debuggers work. Teensy 4 places all user code in RAM. On Teensy 3, you can put a function by specifying FASTRAM. Again, breakpoints like this can be set and enabled/disabled by GDB.

6. On the Teensy 3.2, we could as well use the Flash Patch Block to set and remove SVC calls using patching. Thus, you can dynamically set breakpoints in flash. Teensy 4 doesn't support this, but since it places code in RAM, that's probably not a big deal.

7. It will take over the SVC, software and all fault interrupts. The software interrupt will be "chained" so it will process it's own interrupts and any other interrupts will be sent to the original interrupt handler. The SVC handler will trigger first. It will save the registers and then trigger the software interrupt. It does this because the software interrupt has a lower priority and thus Teensy features like USB will continue to work during a software interrupt, but not during an SVC interrupt which has a higher priority. The software interrupt is chained, meaning that if it is called outside of SVC, it will redirect to the previous software interrupt. This is helpful because the Aduio library uses the software interrupt.

After a sketch is compiled, the `teensy_debug` tool is called to upload the sketch. First, it calls Teensyduino's `teensy_post_compile` to initiate the upload. It waits for that to complete and for Teensy to restart. Then it will find the right serial port and run `gdb` in a separate window. On Mac and Linux, `teensy_debug` is a Python script. On Window, the script has been compiled to an EXE with `pyinstaller`.

TODO / Future considerations
===========================================

Bugs
-------------------------------------------

1. Because stepping is implemented by putting a `SVC` in the next instruction, there are a number of bugs related to `step` and `next`. 

2. `step` may not not step into functions. Stepping won't always work over a return. TeensyDebug traps `bx lr`, `pop {Rmmm, pc}`, `mov pc, Rm` and will usually step properly over these instruction if using gdb `stepi` command. Branch instructions are also interpreted properly most of the time. However gdb `step` and `next` may occasionally get confused and not stop stepping.

3. Port `teensy_debug` to C. Or better yet, integrate it with `teensy_post_compile`.

Future considerations
-------------------------------------------

The `run.command` script should be ported to Windows and Linux.

The serial connection can be anything that supports reading and writing ASCII in sequence. To start it's probably best to use a UART or USB Serial but in theory it could be USB Serial, CAN, network, Raw connection, etc.

GDB can accept "file" commands from Teensy. Thus, teensy could send commands to open, read and write files on the host PC running GDB. This could be useful in many applications.

Right now, GDB runs in a separate window. But in the future, GDB could be piped to Arduino's serial monitor. Both GDB's output and Teensy's serial output could be sent to the display. GDB can receive commands from the Send window. If, in addition to this.

```
[Arduino]      [           ser1] <-- [Teensy & GDB stub]
[Serial ] <--> [gdb proxy      ]        
[Monitor]      [           ser2] <--> [GDB]
```
