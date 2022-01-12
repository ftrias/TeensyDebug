Live debugging on Teensy & GDB support
===========================================
By Fernando Trias, June 2020

This module provides breakpoint support for the Teensy 3.x and 4.x platform from PJRC without need for an external debug interface. The module provides:

1. GDB Remote Serial Protocol stub so that GDB can connect to the Teensy and perform live debugging.

2. Ability to set/clear breakpoints and query registers and memory on running and paused programs.

3. Catch hard crashes and display diagnosics.

Forum discussion: https://forum.pjrc.com/threads/61373-Using-GDB-with-Teensy-without-hardware-debugger-first-Beta?p=243249&posted=1#post243249

License: GNU (see license.txt) and commercial available

GDB background
-------------------------------------------

GDB Remote Serial Protocol provides a simple interface between GDB and a remote target via a serial interface, such as a socket, pipe, UART, USB, etc.

```
[PC running GDB] <--(serial)--> [Teensy GDB stub]
```

Since Teensduino comes with a GDB executable for ARM processors, there is no need to install it. TeensyDebug comes with an installer (see below) that will create a new menu in Arduino that will enable GDB. However, it can also be used stand-alone as a regular Arduino library.

Sample code and usage
-------------------------------------------

This sketch is compiled using the Dual Serial USB option.

```C++
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

When you press Upload, the sketch will be compiled and uploaded to the Teensy. If you have enabled the menu options (see below for installation instructions), GDB will come up in a new window. If not, follow the Manual instructions farther down.

Installing
-------------------------------------------

TeensyDebug can be installed on PlatformIO or Arduino. See the relevant section below.

Installing for PlatformIO
===========================================

Installing on Platform IO involves making a change to your `platform.ini` file. You need to change the `debug_port` item to match whatever port your systems assigns. In windows, use for form `\\.\COM9`.

If you use PlatformIO, none of the other install steps in sections below this are necessary.

```
[env:teensy40]
platform = teensy
board = teensy40
framework = arduino

build_type = debug
; See https://github.com/platformio/platform-teensy/issues/65
build_unflags = -DUSB_SERIAL
build_flags = -DUSB_DUAL_SERIAL
debug_port = /dev/cu.usbmodem61684903 
debug_tool = custom
debug_load_mode = manual
debug_server = 
debug_init_cmds =
  target extended-remote $DEBUG_PORT
  $INIT_BREAK
  define pio_reset_run_target
  interrupt
  tbreak loop
  continue
  end
  define pio_restart_target
  echo Restart is undefined for now.
  end

debug_init_break =
```

Installing for Arduino
===========================================

There is an installer for Mac, Linux and Windows. The installer does these things:

1. Create a directory named TeensyDebug in your library and copy source files there.
2. Customize IDE by copying customized `boards.local.txt` and `platform.local.txt` to `hardware/teensy/avr` directory. These files create the menu options and cause `teensy_debug` to be used to upload the program and run GDB.
3. Copy the `teensy_debug` for your platform to the `hardware/tools` directory.

You can also do these things by hand and not use installer app for your platform.

Installing for Arduino on Mac
-------------------------------------------

This tool requires Python, which is installed by default on Macs. Install by running `install-mac.command` located in the `extras` directory. This script creates a new menu option in Arduino and copies itself to the tools directory. Then it will create a local library with the source files.

Installing for Arduino on Windows
-------------------------------------------

This tool is installed by running `teensy_debug.exe` in `extras` as Administrator. Do this by right-clicking on the file and selecting `Run as administraor`. This program creates a new menu option in Arduino and copies itself to the tools directory. Then it will create a local library with the source files.

Installing for Arduino on Linux
-------------------------------------------

Run `install-linux.sh` in `extras` to install. It assumes your Arduino is installed in `~/arduino`. If this is not so, pass the direction with the `-i=path` option. It will create a local library with the source files.

Installing for Arduino from ZIP file
-------------------------------------------

If you install this library as a ZIP file from the Arduino menu, it will not install the additional menu options for running GDB after uploads. You will either have to do these things manually (see "Installing overview") or you will have to follow the "Running GDB manually" instructions a few sections below.

New menu options
-------------------------------------------

The new menu provides these options:

* Use Dual Serial: If you compile Dual Serial support (or at least two serials), the second USB Serial will be used to communicate with GDB. All optimizations will be turned off.

* Take over Serial: GDB will use the USB Serial to communicate with the Teensy. The library will redefine Serial so that any calls to Serial in your sketch will cause GDB to print your data. All optimizations will be turned off.

* Manual Serial: Compile program and start GDB, but don't connect automatically so you can choose the serial device to use.

* Just compile: Compile with GDB but don't start GDB.

* Off: GDB is not used.

Running GDB manually
-------------------------------------------

If the menu option doesn't work for you, or you are using a physical serial port, you can run GDB manually.

For example, after compiling and uploading the program in the example above, Teensy will have two serial ports. One is the standard one you can view on the Serial Monitor. The other is the one you will connect to. You need to figure out what the device name is (See menu `Tools / Port`). Let's assume it's `/dev/cu.usbmodem61684901`. On Windows, it would be something like `COM20`.

You also need to find the GDB executable that came with Teensyduino. On the Mac it is located in `/Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb`.

Next, find the ELF file created. Arduino puts it in a temporary directory. Fortunately, it is the same directory for the duration of Arduino. If you look at the end of the compile output, you should see multiple mentions of a file ending with `.elf`. For example: `/var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_133762/breakpoint_test.ino.elf`.

Run GDB followed by the ELF file location:

```
$ /Applications/Teensyduino.app/Contents/Java/hardware//tools/arm/bin/arm-none-eabi-gdb /var/folders/j1/8hkyfp_96zl_lgp19b19pbj80000gp/T/arduino_build_133762/breakpoint_test.ino.elf
```

GDB outputs:

```
GNU gdb ...
....
Type "apropos word" to search for commands related to "word".
(gdb)
```

At the prompt use the `target remote` command using the correct port. On Windows it is sometimes necessary to specify serial ports in a long form as: `\\.\COM20`.

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

The `debug` object
===========================================

The library exports an object named `debug` of class `Debug`. Class `Debug` has a number of methods for manipulating the debugging system. In addition, class `Debug` inherits from `Print` so you can output messages to the GDB console.

* `int begin(Stream *device = NULL)`: Initialize the debugging system and use the given device to communicate with GDB. The device must inherit from `Stream`, such as any Serial device.

* `int begin(Stream &device)`: Same as above, but take reference as parameter.

* `int setBreakpoint(void *p, int n=1)`: Set a breakpoint at address.

* `int clearBreakpoint(void *p, int n=1)`: Clear breakpoint at address.

* `void setCallback(void (*c)())`: Set a custom callback function when breakpoint it reached.

* `uint32_t getRegister(const char *reg)`: Get the value of a register.

* `int setRegister(const char *reg, uint32_t value)`: Set a register for when execution resumes.

* `int isGDBConnected()`: Return 1 if GDB has connected. 0 otherwise.

Because `Debug` inherits from `Print`, it supports the usual print functions, such as `print`, `println`, `write`, etc.

GDB supports the target writing files in the PC's file system. This is suppored by the `debug.file_*()` menthods. They follow the standard Posix conventions. If a function returns a negative number, it means failure: The methods of `debug` are:

* `int file_errno()`: Get the last errno error.

* `int file_open(const char *file, int flags = O_CREAT | O_RDWR, int mode = 0644)`

* `int file_close(int fd)`

* `int file_read(int fd, void *buf, unsigned int count)`

* `int file_write(int fd, const void *buf, unsigned int count)`

* `int file_system(const char *buf)`: System calls are disabled by default in GDB. To enable, use the GDB command `set remote system-call-allowed 1`.

For example:
```C++
int fd = debug.file_open("/tmp/test.out");
if (fd < 0) {
  debug.println(debug.file_errno());
}
else {
  debug.file_write(fd, "abc", 3);
  debug.file_close(fd);
}
```

Special GDB commands
-------------------------------------------

GDB provides a command called `monitor` for sending arbitrary text to the Teensy. Right now, this is used to implement pin IO operation, but could easily be expanded. For example, `monitor digitalWrite(13,1)` will flip pin 13 high, which turns on the LED.

These are the commands implemented so far:

* `digitalRead(pin)` -> returns 1 or 0
* `digitalWrite(pin, 1_or_0)`
* `analogRead(pin)` -> returns analog input from pin
* `analogWrite(pin, value)`
* `restart` -> reboot Teensy
* `call(addr [,p1 [,p2 [,p3]]])` -> call a function at address. The function takes only integers (or pointers) as parameters (up to 3) and returns an integer that is displayed back to the user. Address must be numeric. You can get the address of a function with the `p` command as in `p funcname`. For example if `int fx(int x)` is located at `0xc8`, as shown by `p fx`, then the command to return `fx(5)` would be `monitor call(0xc8,5)`. Instead of this, you may want to use GDB's `p` with a function call, as in `p fx(1)`


Internal workings
===========================================

This is how breakpoints are implemented:

1. Using a timer, the Teensy listens for GDB commands from a serial device.

2. When it gets commands like memory queries, memory sets and things that don't require halting, it responds with the data requested. In this way, you can inspect a running program.

3. When it receives a halt command, Teensy will just go into a loop querying for commands and responding. It won't return to it's caller until GDB tells it to do so. Thus, execution of the main thread will stop but interrupts will continue. Because interrupts continue, on the plus side, the Teensy won't die and USB and other features will stay active. On the other hand, sometimes you just the want the system to halt. Perhaps there could be an option to halt all interrupts as well or change the priority. Keeping interrupts going is probably easier for beginners and models what desktop apps do (when an app stops, the OS keeps going).

4. Provide a special hardwired "breakpoint" instruction that you can insert into your code. Each breakpoint will have a flag in RAM to determine if it is enabled or not. If enabled, when execution reaches it, it will execute an interrupt (software or SVC). If disabled, execution just keeps going. Breakpoints are enabled/disabled based on commands received from GDB. This allows for breakpoints in Flash.

5. If a function is placed in RAM, dynamically insert/remove SVC calls in the code. Teensy 4 places all user code in RAM. On Teensy 3, you can put a function in RAM by specifying FASTRAM. Again, breakpoints like this can be set and enabled/disabled by GDB.

6. On the Teensy 3.2, we use the Flash Patch Block to set and remove SVC calls using patching. Thus, you can dynamically set breakpoints in flash. Teensy 4 doesn't support this, but since it places code in RAM, that's probably not a big deal.

7. It will take over the SVC, software and all fault interrupts. The software interrupt will be "chained" so it will process it's own interrupts and any other interrupts will be sent to the original interrupt handler. The SVC handler will trigger first. It will save the registers and then trigger the software interrupt. It does this because the software interrupt has a lower priority and thus Teensy features like USB will continue to work during a software interrupt, but not during an SVC interrupt which has a higher priority. The software interrupt is chained, meaning that if it is called outside of SVC, it will redirect to the previous software interrupt. This is helpful because the Aduio library uses the software interrupt.

After a sketch is compiled, the `teensy_debug` tool is called to upload the sketch. First, it calls Teensyduino's `teensy_post_compile` to initiate the upload. It waits for that to complete and for Teensy to restart. Then it will find the right serial port and run `gdb` in a separate window. On Mac and Linux, `teensy_debug` is a Python script. On Window, the script has been compiled to an EXE with `pyinstaller`.

TODO / Future considerations
===========================================

Bugs
-------------------------------------------

1. Because stepping is implemented by putting a `SVC` in the next instruction, there are a number of bugs related to `step` and `next`. 

2. `step` may not always step into functions. Stepping won't always work over a return. TeensyDebug traps `bx lr`, `pop {Rmmm, pc}`, `mov pc, Rm` and will usually step properly over these instruction if using gdb `stepi` command. Branch instructions are also interpreted properly most of the time. However gdb `step` and `next` may occasionally get confused and stop stepping. Returning from functions on Teensy 3 is very limited.

Future considerations
-------------------------------------------

The `teensy_debug` script should be ported to Windows and Linux. Or better yet, integrate it with `teensy_post_compile`.

The serial connection can be anything that supports reading and writing ASCII in sequence. To start it's probably best to use a UART or USB Serial but in theory it could also be CAN, network socket, USB Raw, MIDI, etc.

Right now, GDB runs in a separate window. But in the future, GDB could be piped to Arduino's serial monitor. Both GDB's output and Teensy's serial output could be sent to the display. GDB can receive commands from the Send window.

```
[Arduino]      [           ser1] <-- [Teensy & GDB stub]
[Serial ] <--> [gdb proxy      ]        
[Monitor]      [           ser2] <--> [GDB]
```
