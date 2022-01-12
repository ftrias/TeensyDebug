20220110

Remove -O0 from code and add PlatformIO instructions.

20200615

Using GDB with Teensy without hardware debugger, first Beta

I put together a library that allows GDB (included in Teensyduino) to perform source-level debugging on the Teensy without debugging hardware (no need for JTAG, SWD, etc). It uses GDB's Remote Serial Protocol to communicate with Teensy over a Serial connection (physical or USB). No extra hardware or software is required. 

This is a beta release for comments and testing. It works on Teensy 4. Tested primarily on Mac.

See https://forum.pjrc.com/threads/61262-Sleeping-to-disable-C_DEBUGEN?p=243070&posted=1#post243070 for previous discussion and this for background: https://forum.pjrc.com/threads/26358-Software-Debugger-Stack.

Highlights:

* Set/remove breakpoints at runtime in most places in your code
* Examine and change memory and registers
* View call stack
* Halt code at any time
* Next/step one instruction at a time

Since real breakpoints are permanently disabled on the Teensy, in order to emulate breakpoints the library uses the SVC interrupt (like I did in TeensyThreads). Since Teensy 4 places most code in RAM, activating a breakpoint replaces the original instructions with SVC calls (and puts the original back later). On Teensy 3.2, setting breakpoints uses the Cortex-M4's features to "patch" parts of flash by pointing it to RAM. 

The library is enabled by including a header, `TeensyDebug.h`. There is a program that adds a menu option that configures Arduino to open GDB automatically after uploading your program. You can also run GDB manually and use `target remote` to connect to the serial port used by the GDB interface.

To read more and try it out visit: http://github.com/ftrias/TeensyDebug

Any comments or code would be greatly appreciated!

20200609

GDB stubs on Teensy
===================

I think live debugging is possible even if C_DEBUGEN cannot be changed. The idea is to use a GDB stub to remotely debug the Teensy and instead of using BKPT, using SVC. This proposal will enable limited breakpoints and live examination of data.

First, I'll summarize the GDB Remote Serial Protocol used by stubs. Then I'll discuss how to use it. I'm writing this here to see if this has already been done and to get feedback.

For background see: https://forum.pjrc.com/threads/26358-Software-Debugger-Stack

GDB stubs
---------

GDB stubs provides a simple interface between GDB and a remote target via a serial interface, such as a socket, pipe, UART, USB, etc. It works like this:

```
[PC running GDB] <--(serial)--> [Teensy GDB stub]
```

Teensyduino comes with a GDB executable that supports ARM. The ELF file contains debugging info if compiled with the "Debug" option that Teensyduino provides. Thus, GDB loads up the ELF and knows all the symbols and sees the source code. GDB queries Teensy for information like memory data and registers as needed. It also sends Teensy breaking and execution commands.

Process
---------

This is how it would work with Teensy:

1. By using interrupts or the EventResponder, the Teensy listens to GDB commands from a serial device.

2. When it gets commands like memory queries, memory sets and things that don't require halting, it responds with the data requested.

3. When it receives a halt command, Teensy will just go into a loop querying for commands and responding. It won't return to it's caller until GDB tells it to do so. Thus, execution of the main thread will stop but interrupts will continue. Because interrupts continue, on the plus side, the Teensy won't die and USB and other features will stay active. On the other hand, sometimes you just the want the system to halt. Perhaps there could be an option to halt all interrupts as well or change the priority. Keeping interrupts going is probably easier for beginners and models what desktop apps do (when an apps stops, the OS keeps going).

So far, this will enable us to do simple live debugging. You can't set breakpoints yet, but you'll be able to stop execution and examine the state.

4. Provide a special "breakpoint" instruction that you can insert into your code. Each breakpoint will have a flag in RAM to determine if it is enabled or not. If enabled, when execution reaches it, it will execute an interrupt (software or SVC). If disabled, execution just keeps going. Breakpoints are enabled/disabled based on commands received from GDB.

So far, this provides fixed hardcoded breakpoints. You can stop the code at any place and examine/change variables, stack, etc.

5. If a function is placed in RAM, you can dynamically insert/remove SVC calls in the code, in the same way that standard debuggers work. Teensy 4 places all user code in RAM. On Teensy 3, you can put a function by specifying FASTRAM. Again, breakpoints like this can be set and enabled/disabled by GDB.

6. On the Teensy 3.2, we could as well use the Flash Patch Block to set and remove SVC calls using patching. Thus, you can dynamically set breakpoints in flash. Teensy 4 doesn't support this, but since it places code in RAM, that's probably not a big deal.

Other considerations
---------

The serial connection can be anything. To start it's probably best to use a UART but in theory it could be USB Serial, CAN, network, Raw connection, etc.

Right now, running GDB will have to be done manually. But in the future, GDB could be piped to Arduino's serial monitor. Both GDB's output and Teensy's serial output could be sent to the display. GDB can receive commands from the Send window. If, in addition to this, we use USB for the serial connection to GDB, then Teensy will have onboard live debugging available with no special setup or hardware required.

```
[Arduino]      [           ser1] <-- [Teensy & GDB stub]
[Serial ] <--> [gdb proxy      ]        
[Monitor]      [           ser2] <--> [GDB]
```

Thus a fairly functional live debugging environment can be created without disabling C_DEBUGEN.