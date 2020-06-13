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

//
// Need to know where RAM starts/stops so we know where
// software breakpoints are possible
//

#ifdef __MK20DX256__
#define RAM_START ((void*)0x1FFF8000)
#define RAM_END   ((void*)0x2FFFFFFF)
#endif

#ifdef __IMXRT1062__
#define RAM_START ((void*)0x00000020)
#define RAM_END   ((void*)0x5FFFFFFF)
#endif


#if defined(GDB_DUAL_SERIAL) && ! defined(CDC2_DATA_INTERFACE)
#error "You must use Dual Serial or Triple Serial to enable GDB on Dual Serial."
#endif

#if defined(GDB_TAKE_OVER_SERIAL) && ! defined(CDC_DATA_INTERFACE)
#error "You must use a USB setup with Serial to enable GDB to take over a Serial interface."
#endif

#if defined(HAS_FP_MAP) || defined(GDB_DUAL_SERIAL) || defined(GDB_TAKE_OVER_SERIAL)
#define REMAP_SETUP
#endif

// If this is used internally, not need to remap
#ifdef GDB_DEBUG_INTERNAL


#else

  #ifdef REMAP_SETUP
  // rename the original setup() because we need to hijack it
  #define setup setup_main
  #endif

  #ifdef GDB_TAKE_OVER_SERIAL
  #define Serial debug
  #endif

#endif

int hcdebug_isEnabled(int n);
int hcdebug_setBreakpoint(int n);

// int debug_setBreakpoint(void *p, int n);
// int debug_clearBreakpoint(void *p, int n);
// void debug_setCallback(void (*c)());
// uint32_t debug_getRegister(const char *reg);

size_t gdb_out_write(const uint8_t *msg, size_t len);

class Debug : public Print {
public:
  int begin(Stream *device = NULL);
  int begin(Stream &device) { return begin(&device); }
  int setBreakpoint(void *p, int n=1);
  int clearBreakpoint(void *p, int n=1);
  void setCallback(void (*c)());
  uint32_t getRegister(const char *reg);
  int setRegister(const char *reg, uint32_t value);
  int restoreRunMode();

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
// #define triggerBreakpoint() { NVIC_SET_PENDING(IRQ_SOFTWARE); }
#define DEBUGRUN __attribute__ ((section(".fastrun"), noinline, noclone ))

#endif
