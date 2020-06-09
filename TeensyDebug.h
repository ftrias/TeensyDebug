#ifndef TEENSY_DEBUG_H
#define TEENSY_DEBUG_H

// is this a Teensy 3.2? If so, we can support hardware interrupts
// and should hijack setup() to set that up.
#ifdef __MK20DX256__
#define HAS_FP_MAP
#endif

// If this is used internally, not need to remap
#ifndef DEBUG_INTERNAL
// rename the original setup() because we need to hijack it
#define setup setup_main
#endif

int hcdebug_isEnabled(int n);
int hcdebug_setBreakpoint(int n);

int debug_setBreakpoint(void *p, int n);
int debug_clearBreakpoint(void *p, int n);
void debug_setCallback(void (*c)());
uint32_t debug_getRegister(const char *reg);

class Debug {
public:
  int setBreakpoint(void *p, int n=1);
  int clearBreakpoint(void *p, int n=1);
  void setCallback(void (*c)());
  uint32_t getRegister(const char *reg);
};
extern Debug debug;

#define breakpoint(n) {if (hcdebug_isEnabled(n)) {asm volatile("svc #0x11");}}
#define breakpoint_enable(n) {hcdebug_setBreakpoint(n);}
#define halt() {asm volatile("svc #0x11");}

#define DEBUGRUN FASTRUN

// set optimizations to 0 so that instructions will match
// C statements; otherwise, gcc will reorg code
#pragma GCC optimize("O0")

#endif
