
20200612

I put together some code that allows GDB to perform source-level debugging on the Teensy without an external debug interface (no need for SWD, etc). It uses GDB's Remote Serial Protocol to communicate with Teensy over a Serial connection (hardware or USB). This is a beta release for comments and testing. It works (mostly) on Teensy 4 and 3.2. Tested on Mac. Highlights:

* Set/remove breakpoints on most places in your code
* Examine and change memory and registers
* View call stack
* Halt code at any time
* Step with next/step working minimally, but not across functions.

In the past, debugging like this has been very challenging because the debugging features of the Teensy are permanently disabled (see https://forum.pjrc.com/threads/26358-Software-Debugger-Stack and https://forum.pjrc.com/threads/61262-Sleeping-to-disable-C_DEBUGEN?p=242721#post242721). But I'm using a trick that doesn't involve the debugging features.

To emulate breakpoints the library first takes over the SVC interrupt. The Teensy 4 places most code in RAM so to activate a breakpoint it just replaces the original instructions with SVC calls. On Teensy 3.2, setting breakpoints uses the Cortex-M4's features to "patch" parts of flash by pointing it to RAM. 

The library is enabled by including the header. On Macs, there is a python script that adds a menu option and configures Arduino to open GDB automatically after uploading your program. On other platforms, you run GDB manually and use `target remote` to connect to the serial port used by the GDB interface. However, the python script is simple and could easily be ported to Windows (or rewritten in C).

To read more and try it out visit: http://github.com/ftrias/TeensyDebug

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