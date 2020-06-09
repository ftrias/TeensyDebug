GDB stubs on Teensy
===================

I gave this some thought over the weekend and I think live debugging is possible even if C_DEBUGEN cannot be changed. The idea is to use a GDB stub to remotely debug the Teensy and instead of using BKPT, using SVC. This proposal will enable limited breakpoints and live examination of data.

First, I'll explain GDB stubs a little. Then I'll discuss how to use it. I'm writing this here to see if this has already been done and to get feedback.

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

5. If a function is placed in RAM with the FASTRAM option, you can dynamically insert/remove SVC calls in the code, in the same way that standard debuggers work. Thus, if you want arbitrary breakpoints in certain code, just specify FASTRAM. Again, breakpoints like this can be set and enabled/disabled by GDB.

In theory, most code can be placed in RAM by changing the gcc compile options, or by linking with an alternative LD file. Perhaps Teensyduino can provide a feature to do this in the future? The Teensy 4 has plenty of RAM to store most programs. You can store some parts, like core code, in flash and all user code in RAM.

6. On the Teensy 3.1/3.2, we could as well use the Flash Patch Block to set and remove SVC calls using patching. Thus, you can dynamically set breakpoints in flash. Teensy 4 doesn't support this.

Other considerations
---------

The serial connection can be anything. To start it's probably best to use a UART but in theory it could be USB Serial, CAN, network, Raw connection, etc.

Right now, running GDB will have to be done manually. But in the future, GDB could be piped to Arduino's serial monitor. Both GDB's output and Teensy's serial output could be sent to the display. GDB can receive commands from the Send window. If, in addition to this, we use USB for the serial connection to GDB, then Teensy will have onboard live debugging available with no special setup or hardware required.

```
[Arduino]     [         s1] <-- [Teensy]
[Serial ] <-> [gdb proxy  ]        
[Monitor]     [         s2] <->  [GDB]
```

If C_DEBUGEN can be disabled, then the Teensy 4 can also have dynamic breakpoints in flash and everything else becomes somewhat simpler to implement.