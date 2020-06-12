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

/** References

https://forum.pjrc.com/threads/26358-Software-Debugger-Stack
https://forum.pjrc.com/threads/28058-Teensyduino-1-22-Features?highlight=C_DEBUGEN
http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0344k/Cegjcacg.html
https://web.eecs.umich.edu/~prabal/teaching/resources/eecs373/ARMv7-M_ARM.pdf
https://os.mbed.com/users/va009039/code/lpcterm2/annotate/5e4107edcbdb/Target2.cpp/
https://sourceforge.net/p/openocd/code/ci/master/tree/src/target/cortex_m.c#l303
https://docs.google.com/viewer?a=v&pid=forums&srcid=MTQ3NDIxMjYyNTI1NDYxNDY0MzcBMTQyMjM2NDMzNTU1NzM4ODY4NjEBQzAtUlFoeFFJMGNKATAuMQEBdjI&authuser=0
https://developer.apple.com/library/archive/documentation/DeveloperTools/gdb/gdb/gdb_33.html
https://elixir.bootlin.com/linux/v3.3/source/arch/mn10300/kernel/gdb-stub.c
https://ftp.gnu.org/old-gnu/Manuals/gdb/html_node/gdb_129.html#SEC134
https://github.com/turboscrew/rpi_stub
https://www.embecosm.com/appnotes/ean4/embecosm-howto-rsp-server-ean4-issue-2.html
https://github.com/redox-os/binutils-gdb/blob/master/gdb/arm-tdep.c
https://github.com/redox-os/binutils-gdb/blob/master/gdb/stubs/i386-stub.c
https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html#Remote-Protocol

*/

#include <Arduino.h>

#define GDB_DEBUG_INTERNAL
#include "TeensyDebug.h"

/**
 * @brief Memory maps missing from headers
 * 
 */

#define FP_CTRL  (*(uint32_t*)0xE0002000)
#define FP_REMAP (*(uint32_t*)0xE0002004)
#define FP_COMP(n) (((uint32_t*)0xE0002008)[n])
#define FP_COMP0 (*(uint32_t*)0xE0002008)
#define FP_COMP1 (*(uint32_t*)0xE000200C)
#define FP_COMP2 (*(uint32_t*)0xE0002010)
#define FP_COMP3 (*(uint32_t*)0xE0002014)
#define FP_COMP4 (*(uint32_t*)0xE0002018)
#define FP_COMP5 (*(uint32_t*)0xE000201C)
#define FP_COMP6 (*(uint32_t*)0xE0002020)
#define FP_COMP7 (*(uint32_t*)0xE0002024)
#define FP_COMP_MASK  0x1FFFFFFC
#define FP_REMAP_MASK 0x1FFFFFF0
#define FP_REMAP_RMPSPT (1<<29)

#define ARM_DHCSR (*(uint32_t*)0xE000EDF0)
#define ARM_DCRSR (*(uint32_t*)0xE000EDF4)
#define ARM_DCRDR (*(uint32_t*)0xE000EDF8)
//#define ARM_DEMCR (*(uint32_t*)0xE000EDFC) // defined in header

#define FP_LAR_UNLOCK_KEY 0xC5ACCE55
#define FP_LAR   (*(unsigned int*) 0xE0000FB0)
#define FP_LSR   (*(unsigned int*) 0xE0000FB4)

//
// Need to know where RAM starts/stops so we know where
// software breakpoints are possible
//

#ifdef __MK20DX256__
#define RAM_START ((void*)0x1FFF8000)
#define RAM_END   ((void*)0x2FFFFFFF)
#endif

#ifdef __IMXRT1062__
#define RAM_START ((void*)0x00000000)
#define RAM_END   ((void*)0x5FFFFFFF)
#endif

/***********************************************
 * 
 * Breakpoint setup
 * 
 */

/**
 * @brief Software RAM breakpoints
 * 
 */

const int sw_breakpoint_count = 32;
void *sw_breakpoint_addr[sw_breakpoint_count];
uint16_t sw_breakpoint_code[sw_breakpoint_count];

int swdebug_clearBreakpoint(void *p) {
  uint32_t addr = ((uint32_t)p) & 0x1FFFFFFE;
  for(int i=0; i<sw_breakpoint_count; i++) {
    if (sw_breakpoint_addr[i] == (void*)addr) {
      sw_breakpoint_addr[i] = 0;
      uint16_t *memory = (uint16_t*)addr;
      *memory = sw_breakpoint_code[i];
      // Serial.print("restore ");Serial.print(addr, HEX);Serial.print("=");Serial.println(*memory, HEX);
      return 0;
    }
  }
  return -1;
}

int swdebug_setBreakpoint(void *p) {
  uint32_t addr = ((uint32_t)p) & 0x1FFFFFFE;
  for(int i=0; i<sw_breakpoint_count; i++) {
    if (sw_breakpoint_addr[i] == 0) {
      sw_breakpoint_addr[i] = (void*)addr;
      uint16_t *memory = (uint16_t*)addr;
      sw_breakpoint_code[i] = *memory;
      // Serial.print("overwrite ");Serial.print(addr, HEX);Serial.print(" from ");Serial.println(*memory, HEX);
      *memory = 0xdf10; // SVC 10
      return 0;
    }
  }
  return -1;
}

int swdebug_isBreakpoint(void *p) {
  uint32_t addr = ((uint32_t)p) & 0x1FFFFFFE;
  for(int i=0; i<sw_breakpoint_count; i++) {
    if (sw_breakpoint_addr[i] == (void*)addr) {
      return 1;
    }
  }
  return 0;
}

#ifdef HAS_FP_MAP

/**
 * @brief Hardware breakpoints, including memory remapping
 * Teensy doesn't support disabling C_DEBUGEN so unfortunately
 * it is not possible to use Cortex debugging features.
 * 
 */

const int hw_breakpoint_count = 6;
void *hw_breakpoints[hw_breakpoint_count];
uint16_t *hw_remap_table;

int hwdebug_clearBreakpoint(void *p, int n) {
  FP_COMP(n) = 0;
  return 0;
}

int hwdebug_setBreakpoint(void *p, int n) {
  if (p == 0) {
    FP_COMP(n) = 0;
//    Serial.print("break0 ");Serial.println(n);
  }
  else {
    uint32_t pc = ((uint32_t)p) & 0x1FFFFFFE;
    if (pc & 0b10) { // must be aligned, so go to next instruction
      // store the first instruction
      pc -= 2;
      remap_table[(n<<1) + 0] = ((uint16_t*)pc)[0];
      remap_table[(n<<1) + 1] = 0xdf10; // svc 10 instruction
    }
    else {
      // store the next instruction
      remap_table[(n<<1) + 0] = 0xdf10; // svc 10 instruction
      remap_table[(n<<1) + 1] = ((uint16_t*)pc)[1];
    }
    
    uint32_t addr = pc & 0x1FFFFFFC;
    FP_COMP(n) = addr | 1;
    breakpoints[n] = p;
//    Serial.print("break ");Serial.print(n);Serial.print(" at ");Serial.print(pc, HEX);Serial.print("=");Serial.println(addr, HEX);
  }
  return 0;
}

void hwdebug_disableBreakpoint(int n) {
  FP_COMP(n) &= 0xFFFFFFFE;
  // Serial.print("break ");Serial.print(n);Serial.println(" disable");
}

void hwdebug_enableBreakpoint(int n) {
  FP_COMP(n) |= 1;
  // Serial.print("break ");Serial.print(n);Serial.println(" enable");
}

int hwdebug_getBreakpoint(void *p) {
  for (int n=1; n<6; n++) {
    if (breakpoints[n]== p) {
      return n;
    }
  }
  return -1;
}

int hwdebug_isBreakpoint(void *p) {
  for (int n=1; n<6; n++) {
    if (breakpoints[n]== p) {
      return 1;
    }
  }
  return 0;
}

#endif

/**
 * @brief Hard coded breakpoints
 * 
 */

const int hc_breakpoint_count = 32;
void *hc_breakpoint_addr[hc_breakpoint_count];
int hc_breakpoint_enabled[hc_breakpoint_count];
int hc_breakpoint_trip = -1;

int debug_isHardcoded(void *addr) {
  // if (addr >= RAM_START && addr <= RAM_END) {
  //   return 0;
  // }
  uint16_t *p = (uint16_t*)addr;
  // SVC 0x11 is reserved for hardcoded breaks
  if (p[0] == 0xdf11) {
    return 1;
  }
  return 0;
}

int hcdebug_clearBreakpoint(int n) {
  hc_breakpoint_enabled[n] = 0;
  return 0;
}

int hcdebug_setBreakpoint(int n) {
  hc_breakpoint_enabled[n] = 1;
  return 0;
}

int hcdebug_isEnabled(int n) {
  return hc_breakpoint_enabled[n];
}

int hcdebug_isBreakpoint(int n) {
  return hc_breakpoint_enabled[n];
}

void hcdebug_tripBreakpoint(int n) {
  hc_breakpoint_trip = n;
}

/**
 * Wrapper functions that call corresponding breakpoint functions.
 * 
 */

/**
 * @brief Initialize the breakpoint system and clear storage
 * 
 */
void debug_initBreakpoints() {
  for(int i=0; i<sw_breakpoint_count; i++) {
    sw_breakpoint_addr[i] = 0;
  }
  for(int i=0; i<hc_breakpoint_count; i++) {
    hc_breakpoint_enabled[i] = 0;
  }
#ifdef HAS_FP_MAP
  hwdebug_clearBreakpoint(0, 0);
  hwdebug_clearBreakpoint(0, 1);
  hwdebug_clearBreakpoint(0, 2);
  hwdebug_clearBreakpoint(0, 3);
  hwdebug_clearBreakpoint(0, 4);
  hwdebug_clearBreakpoint(0, 5);
#endif
}


/**
 * @brief Clear the given breakpoint
 * 
 * @param p Pointer to location with breakpoint
 * @param n Optional slot for hardware breakpoints
 * @return int 0 if success; -1 failure
 */
int debug_clearBreakpoint(void *p, int n) {
  // Serial.print("clear ");Serial.println((int)p,HEX);
  if (p >= RAM_START && p <= RAM_END) {
    return swdebug_clearBreakpoint(p);
  }
  else if (p < (void*)0xF) {
    return hcdebug_clearBreakpoint((int)p);
  }
  else {
#ifdef HAS_FP_MAP
    return hwdebug_clearBreakpoint(p, n);    
#else
    return -1;
#endif
  }
}

/**
 * @brief Set a new breakpoint
 * 
 * @param p Pointer to location to set breakpoint
 * @param n Optional slot for hardware breakpoints
 * @return int 0 = success; -1 = failure
 */
int debug_setBreakpoint(void *p, int n) {
  // Serial.print("set ");Serial.println((int)p,HEX);
  if (p >= RAM_START && p <= RAM_END) {
    return swdebug_setBreakpoint(p);
  }
  else if (p < (void*)0xF) {
    return hcdebug_setBreakpoint((int)p);
  }
  else {
#ifdef HAS_FP_MAP
    return hwdebug_setBreakpoint(p, n);    
#else
    return -1;
#endif
  }
}

/**
 * @brief Return if there is a breakpoint at location
 * 
 * @param p Pointer to location
 * @return int 1 = there is one; 0 = there isn't one
 */
int debug_isBreakpoint(void *p) {
  if (p >= RAM_START && p <= RAM_END) {
    return swdebug_isBreakpoint(p);
  }
  else if (p < (void*)0xF) {
    return hcdebug_isBreakpoint((int)p);
  }
  else {
#ifdef HAS_FP_MAP
    return hwdebug_isBreakpoint(p);    
#else
    return -1;
#endif
  }
}

/*
 * Breakpint handlers
 * 
 * During a breakpoint:
 * 1. The interrupt is initiated and registers are saved.
 * 2. The breakpoint instruction is replaced with the original code.
 * 3. The handler callback is called.
 * 4. A new temporary breakpoint is put in the next instruction.
 * 5. Execution continues and breaks at next instruction.
 * 6. The temporary breakpoint is replaced with origianl code.
 * 7. The original breakpoint is restored.
 * 8. Execution continues.
 */

// breakpoint handler pointer
void (*callback)() = NULL;

// If debugactive=0, this means the actual breakpoint. 
// If debugactive=1, this is the follow on breakpoint.
int debugactive = 0;

// Saved address to restore original breakpoint
uint32_t debugreset = 0;

// Counter for debugging; counts number of breakpoint calls
int debugcount = 0;

// Debug system is enabled?
int debugenabled = 0;

// Are we in a breakpoint or step instruction?
int debugstep = 0;

// Pretty names for breakpoint and fault types
const char *hard_fault_debug_text[] = {
  "debug", "nmi", "hard", "mem", "bus", "usage"
};

// The interrupt call
// 0 = breakpoint
// 1 = nmi
// 2 = hard fault, etc.
uint32_t debug_id = 0;

// Copy of the registers at breakpoint
struct save_registers_struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xPSR;

  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t sp;
} save_registers;

// Structure of ISR stack
struct stack_isr {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xPSR;
};
// Live pointer to stack of ISR. We use this to modify the
// return address and other things
struct stack_isr *stack;

void print_registers() {
  Serial.print("r0=");Serial.println(save_registers.r0);
  Serial.print("r1=");Serial.println(save_registers.r1);
  Serial.print("r2=");Serial.println(save_registers.r2);
  Serial.print("r3=");Serial.println(save_registers.r3);
  Serial.print("r12=");Serial.println(save_registers.r12);
  Serial.print("lr=0x");Serial.println(save_registers.lr, HEX);
  Serial.print("pc=0x");Serial.println(save_registers.pc, HEX);
  Serial.print("r4=");Serial.println(save_registers.r4);
  Serial.print("r5=");Serial.println(save_registers.r5);
  Serial.print("r6=");Serial.println(save_registers.r6);
  Serial.print("r7=");Serial.println(save_registers.r7);
  Serial.print("r8=");Serial.println(save_registers.r8);
  Serial.print("r9=");Serial.println(save_registers.r9);
  Serial.print("r10=");Serial.println(save_registers.r10);
  Serial.print("r11=");Serial.println(save_registers.r11);
  Serial.print("sp=0x");Serial.println(save_registers.sp,HEX);
}

/**
 * @brief Default debug callback
 * 
 */
void debug_action() {
  Serial.println("****DEBUG");
  print_registers();
  Serial.println("****");
}

/**
 * @brief Called by software interrupt to perform breakpoint manipulation
 * during execution and to call the callback.
 * 
 */
void debug_monitor() {
  uint32_t breakaddr = save_registers.pc - 2;

  // Serial.print("break at ");Serial.println(breakaddr, HEX);

  // is this the first breakpoint or are we in a sequence?
  if (debugactive == 0) {
    // Adjust original SP to before the interrupt call
    save_registers.sp += 20;

    // Serial.print("stop at ");Serial.println(breakaddr, HEX);

    if (callback) {
      callback();
    }
    else {
      debug_action();
    }

    if (debug_isHardcoded((void*)breakaddr)) {
    //  Serial.print("hard coded at ");Serial.println(breakaddr, HEX);
      // do nothing because we continue on to next instruction
    }
    else if (debug_isBreakpoint((void*)breakaddr)) {
      // our location is a breakpoint so we need to return it to the original and rerun 
      // the instruction; to prevent a problem, set breakpoint to next instruction
      // and at next break, set it back
      debug_clearBreakpoint((void*)breakaddr, 1);
      // break at next instruction
      debug_setBreakpoint((void*)(save_registers.pc), 0);    
      // set to rerun current instruction
      stack->pc = breakaddr;
      // we need to process the next breakpoint differently
      debugactive = 1;
      // the original breakpoint needs to be put back after next break
      debugreset = breakaddr;
    }
  }
  else {
    // clear the temporary breakpoint
    debug_clearBreakpoint((void*)breakaddr, 0);
    // reset to re-run the instruction
    stack->pc = breakaddr;

    // if we need to reset the original, do so
    if (debugreset) {
      debug_setBreakpoint((void*)debugreset, 1);
      debugreset = 0;
    }

    // are we stepping instruction by instruction?
    if (debugstep) {
      // we're stepping so process any commands and break at next 
      // and stay in this mode
      if (callback) {
        callback();
      }
      else {
        debug_action();
      }
      debug_setBreakpoint((void*)save_registers.pc, 0);
    }
    else {
      // we're not stepping so reset mode
      debugactive = 0;
    }
  }
}

// Save registers within an interrupt. Changes R0 register
#define SAVE_REGISTERS \
    "ldr r0, =stack \n" \
    "str sp, [r0] \n" \
    "ldr r0, =save_registers \n" \
    "ldr r2, [sp, #0] \n" \
    "str r2, [r0, #0] \n" \
    "ldr r2, [sp, #4] \n" \
    "str r2, [r0, #4] \n" \
    "ldr r2, [sp, #8] \n" \
    "str r2, [r0, #8] \n" \
    "ldr r2, [sp, #12] \n" \
    "str r2, [r0, #12] \n" \
    "ldr r2, [sp, #16] \n" \
    "str r2, [r0, #16] \n" \
    \
    "ldr r2, [sp, #20] \n" \
    "str r2, [r0, #20] \n" \
    "ldr r2, [sp, #24] \n" \
    "str r2, [r0, #24] \n" \
    "ldr r2, [sp, #28] \n" \
    "str r2, [r0, #28] \n" \
    \
    "str r4, [r0, #32] \n" \
    "str r5, [r0, #36] \n" \
    "str r6, [r0, #40] \n" \
    "str r7, [r0, #44] \n" \
    "str r8, [r0, #48] \n" \
    "str r9, [r0, #52] \n" \
    "str r10, [r0, #56] \n" \
    "str r11, [r0, #60] \n" \
    "str sp, [r0, #64] \n"

#pragma GCC push_options
#pragma GCC optimize ("O0")

void (*original_software_isr)() = NULL;
void (*original_svc_isr)() = NULL;

/**
 * @brief Called by software interrupt. Perform chaining or
 * call handler.
 * 
 */
__attribute__((noinline, naked))
void debug_call_isr() {
  // Are we in debug mode? If not, just jump to original ISR
  if (debugenabled == 0) {
    if (original_software_isr) {
      asm volatile("mov pc,%0" : : "r" (original_software_isr));
    }
    return;
  }
  asm volatile(
    "ldr r0, =stack \n"
    "str sp, [r0] \n"
    "push {lr} \n");
  debug_monitor();              // process the debug event
  debugenabled = 0;
  asm volatile("pop {pc}");
}

/**
 * @brief Called by SVC ISR to trigger software interrupt
 * 
 */
void debug_call_isr_setup() {
  debugcount++;
  debugenabled = 1;
  // process in lower priority so services can keep running
  NVIC_SET_PENDING(IRQ_SOFTWARE); 
}

/**
 * @brief SVC handler. Save registers and handle breakpoint.
 * 
 */
__attribute__((noinline, naked))
void svcall_isr() {
  asm volatile(SAVE_REGISTERS);
  asm volatile("push {lr}");
#if 0
  uint8_t *memory = (uint8_t*)(stack->pc - 2);
  if ((*memory) & 0xFFF0 == 0xdf10|| debug_isBreakpoint(memory)) {
    debug_call_isr_setup();
  }
  else {
    if (original_svc_isr) {
      asm volatile("pop {lr}");
      asm volatile("mov pc,%0" : : "r" (original_svc_isr));
    }
  }
#else
  debug_call_isr_setup();
#endif
  asm volatile("pop {pc}");
}

/**
 * @brief Table used by FP_MAP to map memory to breakpoints
 * 
 */
__attribute__((naked))
void svc_call_table() {
  asm volatile(
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
    "svc #0x10 \n"
    "nop \n"
 );  
}

#pragma GCC pop_options

/**
 * @brief Return register value for a text representation.
 * 
 * @param reg Text representation 'r0', 'r1', etc.
 * @return uint32_t Value of register
 */
uint32_t debug_getRegister(const char *reg) {
  if (reg[0] == 'r') {
    if (reg[2] == 0) { // r0-r9
      switch(reg[1]) {
        case '0': return save_registers.r0;
        case '1': return save_registers.r1;
        case '2': return save_registers.r2;
        case '3': return save_registers.r3;
        case '4': return save_registers.r4;
        case '5': return save_registers.r5;
        case '6': return save_registers.r6;
        case '7': return save_registers.r7;
        case '8': return save_registers.r8;
        case '9': return save_registers.r9;
      }
    }
    else if (reg[1] == '1') { // r10-r12
      switch(reg[1]) {
        case '0': return save_registers.r10;
        case '1': return save_registers.r11;
        case '2': return save_registers.r12;
      }
    }
  }
  else if (strcmp(reg, "lr")==0) return save_registers.lr;
  else if (strcmp(reg, "pc")==0) return save_registers.pc;
  else if (strcmp(reg, "sp")==0) return save_registers.sp;
  else if (strcmp(reg, "cpsr")==0) return save_registers.xPSR;
  return -1;
}

/**
 * Fault debug messages
 */

void flash_blink(int n) {
  volatile int p = 0;
  pinMode(13, OUTPUT);
  while(1) {
    for(int c=0; c<n; c++) {
      for(int i=0; i<20000000; i++) {p++;}
      digitalWrite(13, HIGH);
      for(int i=0; i<20000000; i++) {p++;}
      digitalWrite(13, LOW);
    }
    for(int i=0; i<100000000; i++) {p++;}
  }
}

int debug_crash = 0;

/**
 * @brief Default handler for faults
 * 
 * @param n 
 */
void hard_fault_debug(int n) {
  // if (debug_crash) flash_blink(n);
  Serial.print("****FAULT ");
  Serial.println(hard_fault_debug_text[n]);
  Serial.print("r0=");Serial.println(stack->r0, HEX);
  Serial.print("r1=");Serial.println(stack->r1, HEX);
  Serial.print("r2=");Serial.println(stack->r2, HEX);
  Serial.print("r3=");Serial.println(stack->r3, HEX);
  Serial.print("r12=");Serial.println(stack->r12, HEX);
  Serial.print("lr=0x");Serial.println(stack->lr, HEX);
  Serial.print("pc=0x");Serial.println(stack->pc, HEX);
  stack->pc += 2;
  debug_crash = 1;
}

// uint32_t hard_fault_debug_addr = (uint32_t)hard_fault_debug;

#pragma GCC push_options
#pragma GCC optimize ("O0")

// Save registers during fault and call default handler
#define fault_isr_stack(fault) \
  asm volatile("ldr r0, =stack \n str sp, [r0]"); \
  asm volatile("push {lr}"); \
  hard_fault_debug(fault); \
  asm volatile("pop {pc}")

/**
 * @brief Trap faults
 * 
 */
__attribute__((noinline, naked)) void nmi_isr(void) { fault_isr_stack(1); }
__attribute__((noinline, naked)) void hard_fault_isr(void) { fault_isr_stack(2); }
__attribute__((noinline, naked)) void memmanage_fault_isr(void) { fault_isr_stack(3); } 
__attribute__((noinline, naked)) void bus_fault_isr(void)  { fault_isr_stack(4); }
__attribute__((noinline, naked)) void usage_fault_isr(void)  { fault_isr_stack(5); }

#pragma GCC pop_options

/**
 * Initialization code
 * 
 */

void dumpmem(void *mem, int sz) {
  Serial.print((uint32_t)mem, HEX);
  Serial.print("=");
  for(int i=0; i<sz; i++) {
    Serial.print(((uint8_t*)mem)[i], HEX);
    Serial.print(":");
  }
  Serial.println();
}

// store the address of the stack pointer where we pre-allocate space since the remap
// table must be in ram above 0x20000000 and this ram is in the stack area.
uint32_t save_stack;

/**
 * @brief Initialize debugging system.
 * 
 */
void debug_init() {

#ifdef HAS_FP_MAP
  // find next aligned block in the stack
  uint32_t xtable = (save_stack + 0x100) & 0xFFFFFFC0;
  // copy table
  uint32_t *sourcemem = (uint32_t*)(((uint32_t)svc_call_table & 0xFFFFFFFE));
  uint32_t *destmem = (uint32_t*)xtable;
  for(int i=0; i<6; i++) {
    destmem[i] = sourcemem[i];
  }
  // enable the remap, but don't assign any yet
  FP_LAR = FP_LAR_UNLOCK_KEY; // doesn't do anything, but might in some other processors
  FP_REMAP = xtable;
  remap_table = (uint16_t *)xtable;
  FP_CTRL = 0b11;

  // delay(3000);
//  Serial.println(FP_CTRL, HEX);
//  dumpmem(sourcemem, 32);
//  dumpmem(destmem, 32);
//  dumpmem(xtable, 32);
#endif

  _VectorsRam[2] = nmi_isr;
  _VectorsRam[3] = hard_fault_isr;
  _VectorsRam[4] = memmanage_fault_isr;
  _VectorsRam[5] = bus_fault_isr;
  _VectorsRam[6] = usage_fault_isr;

  original_svc_isr = _VectorsRam[11];
  _VectorsRam[11] = svcall_isr;

  // chaing the software ISR handler
  original_software_isr = _VectorsRam[IRQ_SOFTWARE + 16];
  _VectorsRam[IRQ_SOFTWARE + 16] = debug_call_isr;
  NVIC_SET_PRIORITY(IRQ_SOFTWARE, 208); // 255 = lowest priority
  NVIC_ENABLE_IRQ(IRQ_SOFTWARE);

  debug_initBreakpoints();
}

void gdb_init(Stream *device);

#ifdef REMAP_SETUP

// We will rename the original setup() to this by using a #define
void setup_main();

void debug_setup_auto() {
#ifdef GDB_DUAL_SERIAL
  gdb_init(&SerialUSB1);
#endif
#ifdef GDB_TAKE_OVER_SERIAL
  gdb_init(&Serial);
#endif
}

/*
 * The setup function must allocate space on the stack for the remap table; this space must
 * reside above 0x20000000 and this area is reserved in the stack. This is OK because the function
 * calling setup() is main(), which never returns. So taking a chunk of stack won't affect it. If
 * main() does ever want to return, it will have to dealloate this memory.
 */
void __attribute__((naked)) setup() {

#ifdef HAS_FP_MAP
  asm volatile("sub sp, #512");                       // allocate 512 bytes so we have room to align data
  asm volatile("mov %0, sp" : "=r" (save_stack) );    // save the location
#endif
  asm volatile("push {lr}");                          // save the return address
  debug_setup_auto();
  setup_main();                                       // call the "real" setup function
  asm volatile("pop {pc}");                           // get original return address
}

#endif

/**
 * @brief Initialize both debugger and GDB
 * 
 * @param device Device to use; inherits from Stream; if NULL use Serial
 * @return int 
 */
int debug_begin(Stream *device) {
  debug_init();                                       // perform debug initialization
  gdb_init(device);
  return 1;
}

/**
 * Class
 */

int Debug::begin(Stream *device) { return debug_begin(device); }
int Debug::setBreakpoint(void *p, int n) { return debug_setBreakpoint(p, n); }
int Debug::clearBreakpoint(void *p, int n) { return debug_clearBreakpoint(p, n); }
void Debug::setCallback(void (*c)()) { callback = c; }
uint32_t Debug::getRegister(const char *reg) { return debug_getRegister(reg); }

Debug debug;
