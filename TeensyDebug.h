/**
 * @file TeensyDebug.cpp
 * @author Fernando Trias
 * @brief Implement GDB stub using TeensyDebug interface
 * @version 0.1
 * @date 2020-06-09
 * 
 * @copyright Copyright (c) 2020 Fernando Trias
 * 
 */

#ifndef TEENSY_DEBUG_H
#define TEENSY_DEBUG_H

// is this a Teensy 3.2? If so, we can support hardware interrupts
// and should hijack setup() to set that up.
#ifdef __MK20DX256__
#define HAS_FP_MAP
#endif

// If this is used internally, not need to remap
#ifndef GDB_DEBUG_INTERNAL

#ifdef HAS_FP_MAP
// rename the original setup() because we need to hijack it
#define setup setup_main
#endif

#ifdef GDB_TAKE_OVER_SERIAL
#define Serial debug
#endif

#endif

int hcdebug_isEnabled(int n);
int hcdebug_setBreakpoint(int n);

int debug_setBreakpoint(void *p, int n);
int debug_clearBreakpoint(void *p, int n);
void debug_setCallback(void (*c)());
uint32_t debug_getRegister(const char *reg);

size_t gdb_out_write(const uint8_t *msg, size_t len);

class Debug : public Print {
public:
  int begin(Stream *device = NULL);
  int begin(Stream &device) { return begin(&device); }
  int setBreakpoint(void *p, int n=1);
  int clearBreakpoint(void *p, int n=1);
  void setCallback(void (*c)());
  uint32_t getRegister(const char *reg);

  virtual size_t write(uint8_t b) { 
    return write(&b, 1);
  };
	virtual size_t write(const uint8_t *buffer, size_t size) {
    return gdb_out_write(buffer, size);
  }
	virtual int availableForWrite(void)		{ return 128; }
	virtual void flush() { }	
};

extern Debug debug;

#define breakpoint(n) {if (hcdebug_isEnabled(n)) {asm volatile("svc #0x11");}}
#define breakpoint_enable(n) {hcdebug_setBreakpoint(n);}
#define halt() {asm volatile("svc #0x11");}

#define DEBUGRUN __attribute__ ((section(".fastrun"), noinline, noclone ))

#endif
