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
http://engold.ui.ac.ir/~nikmehr/Appendix_B2.pdf
http://wcours.gel.ulaval.ca/GIF1001/docs/arm-instructionset.pdf
https://web.eecs.umich.edu/~prabal/teaching/eecs373-f10/readings/ARMv7-M_ARM.pdf

*/

#include <Arduino.h>

#define GDB_DEBUG_INTERNAL
#include "TeensyDebug.h"

// #define DISPLAY_HARD_FAULT

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

#define ADDRESS_MASK  0xFFFFFFFE
#define ADDRESS_MASK2 0xFFFFFFFC

#define CPU_FLAG_FPCA 2

#ifdef __ARM_PCS_VFP
// reserve space for 8 register, 16 floats, float stat and spacer
#define ISR_STACK_SIZE ((8+16+1+1)*4)
#else
#define ISR_STACK_SIZE (8*4)
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
  uint32_t addr = ((uint32_t)p) & ADDRESS_MASK;
  for(int i=0; i<sw_breakpoint_count; i++) {
    if (sw_breakpoint_addr[i] == (void*)addr) {
      sw_breakpoint_addr[i] = 0;
      uint16_t *memory = (uint16_t*)addr;
      *memory = sw_breakpoint_code[i];
      // Serial.print("clear bkpt; restore ");Serial.print(addr, HEX);Serial.print("=");Serial.println(*memory, HEX);
      return 0;
    }
  }
  return -1;
}

int swdebug_setBreakpoint(void *p) {
  uint32_t addr = ((uint32_t)p) & ADDRESS_MASK;
  for(int i=0; i<sw_breakpoint_count; i++) {
    if (sw_breakpoint_addr[i] == 0) {
      sw_breakpoint_addr[i] = (void*)addr;
      uint16_t *memory = (uint16_t*)addr;
      sw_breakpoint_code[i] = *memory;
      // Serial.print("set brkt; overwrite ");Serial.print(addr, HEX);Serial.print(" from ");Serial.println(*memory, HEX);
      *memory = 0xdf10; // SVC 10
      return 0;
    }
  }
  return -1;
}

int swdebug_isBreakpoint(void *p) {
  uint32_t addr = ((uint32_t)p) & ADDRESS_MASK;
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

int hwdebug_clearAllBreakpoints() {
  for (int n=1; n<6; n++) {
    hw_breakpoints[n] = 0;
    FP_COMP(n) = 0;
  }
  return 0;
}

int hwdebug_getBreakpoint(void *p) {
  for (int n=1; n<6; n++) {
    if (hw_breakpoints[n] == p) {
      return n;
    }
  }
  return -1;
}

int hwdebug_nextBreakpoint() {
  for (int n=1; n<6; n++) {
    if (hw_breakpoints[n] == 0) {
      return n;
    }
  }
  return -1;
}

int hwdebug_isBreakpoint(void *p) {
  for (int n=1; n<6; n++) {
    if (hw_breakpoints[n] == p) {
      return 1;
    }
  }
  return 0;
}

int hwdebug_clearBreakpoint(void *p) {
  int n = hwdebug_getBreakpoint(p);
  if (n < 0) return -1;
  FP_COMP(n) = 0;
  hw_breakpoints[n] = 0;
  // Serial.print("hw clear ");Serial.print(n);Serial.print(" at ");Serial.println((int)p, HEX);
  return 0;
}

int hwdebug_setBreakpoint(void *p) {
  int n = hwdebug_getBreakpoint(p);
  if (n > 0) {
    // Serial.print("set existing hw breakpoint at ");Serial.println((int)p, HEX);
    return 0;
  }

  n = hwdebug_nextBreakpoint();
  if (n < 0) {
    // Serial.print("out of breakpoints for ");Serial.println((int)p, HEX);
    return -1;
  }

  uint32_t pc = ((uint32_t)p) & ADDRESS_MASK;
  if (pc & 0b10) { // must be aligned, so go to next instruction
    // store the first instruction
    pc -= 2;
    hw_remap_table[(n<<1) + 0] = ((uint16_t*)pc)[0];
    hw_remap_table[(n<<1) + 1] = 0xdf10; // svc 10 instruction
  }
  else {
    // store the next instruction
    hw_remap_table[(n<<1) + 0] = 0xdf10; // svc 10 instruction
    hw_remap_table[(n<<1) + 1] = ((uint16_t*)pc)[1];
  }
  
  uint32_t addr = pc & ADDRESS_MASK2;
  FP_COMP(n) = addr | 1;
  hw_breakpoints[n] = p;
  // Serial.print("hw break ");Serial.print(n);Serial.print(" at ");Serial.print(pc, HEX);Serial.print("=");Serial.println(addr, HEX);

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
  // SVC 0x12 is Ctrl-C trap
  if (p[0] == 0xdf11 || p[0] == 0xdf12) {
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
  hwdebug_clearAllBreakpoints();
#endif
}


/**
 * @brief Clear the given breakpoint
 * 
 * @param p Pointer to location with breakpoint
 * @param n Optional slot for hardware breakpoints
 * @return int 0 if success; -1 failure
 */
int debug_clearBreakpoint(void *p) {
  // Serial.print("clear ");Serial.println((int)p,HEX);
  if (p >= RAM_START && p <= RAM_END) {
    return swdebug_clearBreakpoint(p);
  }
  // else if (p < (void*)0xF) {
  //   return hcdebug_clearBreakpoint((int)p);
  // }
  else {
#ifdef HAS_FP_MAP
    return hwdebug_clearBreakpoint(p);    
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
int debug_setBreakpoint(void *p) {
  // Serial.print("set ");Serial.println((int)p,HEX);
  if (p >= RAM_START && p <= RAM_END) {
    return swdebug_setBreakpoint(p);
  }
  // else if (p < (void*)0xF) {
  //   return hcdebug_setBreakpoint((int)p);
  // }
  else {
#ifdef HAS_FP_MAP
    return hwdebug_setBreakpoint(p);    
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
  // else if (p < (void*)0xF) {
  //   return hcdebug_isBreakpoint((int)p);
  // }
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

// Counter for debugging; counts number of breakpoint calls
int debugcount = 0;

// Debug system is enabled?
int debugenabled = 0;

// Are we in a breakpoint or step instruction?
int debugstep = 0;

// Restore registers before returning?
int debugrestore = 0;

// Pretty names for breakpoint and fault types
const char *hard_fault_debug_text[] = {
  "debug", "break", "nmi", "hard", "mem", "bus", "usage"
};

// The interrupt call
// 0 = breakpoint
// 1 = nmi
// 2 = hard fault, etc.
uint32_t debug_id = 0;

// Debug tracing - not used by code
int debug_trace = 0;

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

/**
 * @brief Display registers for debugging of the debugger
 * 
 */
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
 * @brief Return instruction width in bytes of the instruction *p
 * 
 * @param p Pointer to instruction
 * @return int Number of bytes (usually 2)
 */
int instructionWidth(void *p) {
  uint16_t prefix = (*(uint16_t*)p) & 0xF800;
  if (prefix == 0xF000) return 2;
  if (prefix == 0xE800) return 2;
  if (prefix == 0xF000) return 2;
  if (prefix == 0xF800) return 2;
  return 1;
}

/**
 * @brief Parse register instruction bits and return register value
 * 
 * @param x Register bits from instruction
 * @return uint32_t Value of register
 */
uint32_t getRegisterNum(int x) {
  switch(x) {
    case  0: return save_registers.r0;
    case  1: return save_registers.r1;
    case  2: return save_registers.r2;
    case  3: return save_registers.r3;
    case  4: return save_registers.r4;
    case  5: return save_registers.r5;
    case  6: return save_registers.r6;
    case  7: return save_registers.r7;
  }
  return 0;
}

/**
 * @brief Count the number of registers in instruction. Used by "pop" and "push"
 * 
 * @param x Bits from instruction
 * @return int Number of registers to pop or push
 */
int countBits(int x) {
  return
    ((x & (1<<0)) >> 0) +
    ((x & (1<<1)) >> 1) +
    ((x & (1<<2)) >> 2) +
    ((x & (1<<3)) >> 3) +
    ((x & (1<<4)) >> 4) +
    ((x & (1<<5)) >> 5) +
    ((x & (1<<6)) >> 6) +
    ((x & (1<<7)) >> 7);
}

/**
 * @brief If pointer is "return" instruction, return the address where
 * it will return.
 * 
 * @param p Pointer to instruction
 * @return void* Address to return to
 */
void *instructionReturn(void *p) {
  uint16_t inst = *(uint16_t*)p;
  uint16_t prefix = inst >> 8;
  // mov pc, Rx
  // Serial.print("ret? prefix ");Serial.println(prefix);
  if (prefix == 0b01000110 && ((inst & 0b111) == 0b111)) {
    uint32_t reg = getRegisterNum((inst >> 3) & 0b111);
    // Serial.print("mov pc ");Serial.println(reg);
    return (void*)reg;
  }
  if (prefix == 0b10111101) { // pop {Rxxx, pc}
    int regs = countBits(inst & 0xFF);
    uint32_t *memory = (uint32_t*)save_registers.sp;
    // Serial.print("pop pc instr ");Serial.println(inst, HEX);
    // Serial.print("regs ");Serial.println(regs);
    // Serial.print("pop pc at ");Serial.println(memory[regs], HEX);
    // return 0;
    return (void*)memory[regs]; 
  }
  if (inst == 0x4770) { // bx lr
    // Serial.print("bx lr=");Serial.println(save_registers.lr);
    return (void*)save_registers.lr; 
  }
  return 0;
}

/**
 * @brief If pointer is branch, return the branching address. Works for conditional
 * and explicit 2-byte and 4-byte branches.
 * 
 * @param p Instruction pointer
 * @param bx Flag set if instruction is BX (modifies LR)
 * @return void* Address branching to
 */
void *instructionBranch(void *p, int *bx) {
  *bx = 0;
  uint16_t inst = *(uint16_t*)p;

  // BX or BLX
  if ((inst & 0xFF00) == 0x4700) {
    int reg = (inst >> 3) & 0b1111;
    uint32_t addr = getRegisterNum(reg);
    *bx = 1;
    // Serial.print("BX ");Serial.println(addr, HEX);
    return (void*)(addr);
  }
  // B
  if ((inst & 0xF800) == 0xC000) {
    int16_t offset = inst & 0x7F;
    if (offset & 0x400) { // sign extend negative number
      offset |= 0xF800;
    }
    // Serial.print("B ");Serial.println(save_registers.pc + offset, HEX);
    return (void*)(save_registers.pc + offset);
  }
  // BL prefix
  else if ((inst & 0xF800) == 0xF000) {
    int offset1 = inst & 0x3F;
    uint16_t inst2 = ((uint16_t*)p)[1];
    int offset2 = inst2 & 0x7FF;
    int32_t offset = (offset1 << 11) + offset2;
    if (offset &   0x10000) {
      offset |= 0xFFFE0000;
    }
    offset <<= 1;
    // Serial.print("BL prefix ");Serial.println(save_registers.pc + offset + 4, HEX);
    return (void*)(save_registers.pc + offset + 4);
  }
  // B conditional
  else  if ((inst & 0xF000) == 0xD000) {
    int8_t offset = inst & 0xFF;
    // Serial.print("B cond ");Serial.println(save_registers.pc + offset, HEX);
    return (void*)(save_registers.pc + offset);
  }
  return 0;
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

// Saved address to restore original breakpoint
uint32_t debugreset = 0;
uint32_t temp_breakpoint = 0;
uint32_t temp_breakpoint2 = 0;

/**
 * @brief Set a breakpoint at the next instructions, taking into account returns
 * and branches.
 * 
 * @param breakaddr Location of instruction
 * @param nextaddr Location of next instruction (as given by PC)
 */
void setBreakPointNext(uint32_t breakaddr, uint32_t nextaddr) {
#ifdef HAS_FP_MAP
  // For some unknown reason, FP doesn't work across returns like pop {pc},
  // etc.
  if (0) { }
#else
  void *ret = instructionReturn((void*)breakaddr);
  // is this a return of some sort?
  if (ret) {
    temp_breakpoint = (uint32_t)ret;
    // Serial.print("return to ");Serial.println(temp_breakpoint, HEX);
  }
#endif
  else {
    int bx;
    // are we branching?
    void *b = instructionBranch((void*)breakaddr, &bx);
    if (b) {
      temp_breakpoint2 = (uint32_t)b;
      // Serial.print("branch to ");Serial.println(temp_breakpoint2, HEX);
      debug_setBreakpoint((void*)temp_breakpoint2);
    }
    // is 32 bits wide?
    if (instructionWidth((void*)breakaddr) == 2) {
    // Serial.print("32-bit instruction at ");Serial.println(breakaddr, HEX);
      temp_breakpoint = nextaddr + 2;
    }
    else {
      temp_breakpoint = nextaddr;
    }
  }
  debug_setBreakpoint((void*)temp_breakpoint);
}

/**
 * @brief Called by software interrupt to perform breakpoint manipulation
 * during execution and to call the callback.
 * 
 */
void debug_monitor() {
  uint32_t nextaddr = save_registers.pc;
  uint32_t breakaddr = save_registers.pc - 2;

  // Serial.print("break at ");Serial.println(breakaddr, HEX);
  // print_registers();

  if (debug_isHardcoded((void*)breakaddr)) {
    // do nothing for hardcoded interrupts; we continue on next instruction
  }
  // else if (debug_id == 3) { // break cause by Ctrl-C
  //   // Serial.print("manual breakpoint at ");Serial.println(breakaddr, HEX);
  //   // do nothing because we continue on to next instruction
  // }
  // regular breakpoint
  else if (debug_id == 0) {
    save_registers.pc = breakaddr; // gdb expects address at breakpoint in pc
    // set to rerun current instruction
    stack->pc = breakaddr;

    // gdb will clear the current breakpoint but we do this first so disassembly holds 
    // original code
    debug_clearBreakpoint((void*)breakaddr);
    // clear the temporary breakpoints
    if (temp_breakpoint) {
      debug_clearBreakpoint((void*)temp_breakpoint);
      temp_breakpoint = 0;
    }
    if (temp_breakpoint2) {
      debug_clearBreakpoint((void*)temp_breakpoint2); 
      temp_breakpoint2 = 0;
    }
  }

  // Adjust original SP to before the interrupt call to remove ISR's stack entries
  // so GDB has correct stack. The actual stack pointer will get restored
  // to this value when the interrupt returns.
  save_registers.sp += ISR_STACK_SIZE;

  if (callback) {
    callback();
  }
  else {
    debug_action();
  }

  debug_id = 0;

  if (debugstep) {
    // break at next instruction
    setBreakPointNext(breakaddr, nextaddr);
    debugstep = 0;
    // the original breakpoint needs to be put back after next break
    // debugreset = breakaddr;
  }

  // if we need to reset the original, do so
  if (debugreset) {
    debug_setBreakpoint((void*)debugreset);
    debugreset = 0;
  }
}

/**
 * @brief Macros to save/restore registers from stack
 * 
 */
#define SAVE_STACK \
    "ldr r0, =stack \n" \
    "str sp, [r0] \n"

// Save registers within an interrupt. Changes R0 register
#define SAVE_REGISTERS \
    "ldr r0, =stack \n" \
    "ldr r1, [r0] \n " \
    "ldr r0, =save_registers \n" \
    "ldr r2, [r1, #0] \n" \
    "str r2, [r0, #0] \n" \
    "ldr r2, [r1, #4] \n" \
    "str r2, [r0, #4] \n" \
    "ldr r2, [r1, #8] \n" \
    "str r2, [r0, #8] \n" \
    "ldr r2, [r1, #12] \n" \
    "str r2, [r0, #12] \n" \
    "ldr r2, [r1, #16] \n" \
    "str r2, [r0, #16] \n" \
    \
    "ldr r2, [r1, #20] \n" \
    "str r2, [r0, #20] \n" \
    "ldr r2, [r1, #24] \n" \
    "str r2, [r0, #24] \n" \
    "ldr r2, [r1, #28] \n" \
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
    "str r1, [r0, #64] \n"

// Restore all registers except SP
#define RESTORE_REGISTERS \
    "ldr r0, =stack \n" \
    "ldr r1, [r0] \n " \
    "ldr r0, =save_registers \n" \
    "ldr r2, [r0, #0] \n" \
    "str r2, [r1, #0] \n" \
    "ldr r2, [r0, #4] \n" \
    "str r2, [r1, #4] \n" \
    "ldr r2, [r0, #8] \n" \
    "str r2, [r1, #8] \n" \
    "ldr r2, [r0, #12] \n" \
    "str r2, [r1, #12] \n" \
    "ldr r2, [r0, #16] \n" \
    "str r2, [r1, #16] \n" \
    \
    "ldr r2, [r0, #20] \n" \
    "str r2, [r1, #20] \n" \
    "ldr r2, [r0, #24] \n" \
    "str r2, [r1, #24] \n" \
    "ldr r2, [r0, #28] \n" \
    "str r2, [r1, #28] \n" \
    \
    "ldr r4, [r0, #32] \n" \
    "ldr r5, [r0, #36] \n" \
    "ldr r6, [r0, #40] \n" \
    "ldr r7, [r0, #44] \n" \
    "ldr r8, [r0, #48] \n" \
    "ldr r9, [r0, #52] \n" \
    "ldr r10, [r0, #56] \n" \
    "ldr r11, [r0, #60] \n"

void (*original_software_isr)() = NULL;
void (*original_svc_isr)() = NULL;

/**
 * @brief Called by software interrupt. Perform chaining or
 * call handler.
 * 
 */
__attribute__((noinline, naked))
void debug_call_isr() {
  __disable_irq();
  asm volatile(SAVE_STACK);
  asm volatile(SAVE_REGISTERS);
  __enable_irq();
  asm volatile("push {lr}");
  NVIC_CLEAR_PENDING(IRQ_SOFTWARE);

  // Are we in debug mode? If not, just jump to original ISR
  if (debugenabled == 0) {
#if 1
    if (original_software_isr) {
      // asm volatile("ldr r0, =original_software_isr");
      // asm volatile("ldr r0, [r0]");
      // asm volatile("mov pc, r0");
      asm volatile("pop {lr}");
      asm volatile("mov pc, %0" : : "r" (original_software_isr));
    }
#endif
    return;
  }

  if (debugenabled == 2) { // halt permenantly
    while(1) { yield(); }
  }

  debug_monitor();              // process the debug event
  debugenabled = 0;
  // Serial.print("restore regs=");Serial.println(debugrestore);

  // restore registers if they have been changed by gdb
  if (debugrestore) {
    debugrestore = 0;
    asm volatile("pop {r12}");
    __disable_irq();
    asm volatile(RESTORE_REGISTERS);
    __enable_irq();
    asm volatile("mov lr, r12");
    asm volatile("bx lr");
  }
  else {
    asm volatile("pop {pc}");
  }
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

#if 1
uint32_t lastpc;

int testOurSVC() {
  uint16_t *memory = (uint16_t*)(lastpc);
  if (((*memory) & 0xFFF0) == 0xdf10 || debug_isBreakpoint(memory)) {
    return 1;
  }
  return 0;
}
#endif

/**
 * @brief SVC handler. Save registers and handle breakpoint.
 * 
 */
__attribute__((noinline, naked))
void svcall_isr() {
#if 1
  // get the PC that triggered this
  // subtract width of svc instruction (which is 2)
  // is it one of our svcs?
  asm volatile(
    "ldr r0, [sp, #24] \n"
    "sub r0, #2 \n"
    "ldr r1, =lastpc \n"
    "str r0, [r1] \n"
    "push {lr}"
  );
  if (testOurSVC()) {
    debug_call_isr_setup();
    asm volatile("pop {pc}");
  }
  else {
    if (original_svc_isr) {
      asm volatile("pop {lr}");
      asm volatile("mov pc, %0" : : "r" (original_svc_isr));
    }
    asm volatile("pop {pc}");
  }
#else
  asm volatile("push {lr}");
  debug_call_isr_setup();
  asm volatile("pop {pc}");
#endif
}

/**
 * @brief Table used by FP_MAP to map memory to breakpoints. This will
 * get copied to RAM and serve as reference. Probably not really needed.
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
      switch(reg[2]) {
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
 * @brief Set register
 * 
 * @param reg Text register as in 'r0', 'r1', etc.
 * @param value Value to set
 * @return int 0 if failed; 1 if success
 */
int debug_setRegister(const char *reg, uint32_t value) {
  debugrestore = 1;
  if (reg[0] == 'r') {
    if (reg[2] == 0) { // r0-r9
      switch(reg[1]) {
        case '0': save_registers.r0 = value; return 1;
        case '1': save_registers.r1 = value; return 1;
        case '2': save_registers.r2 = value; return 1;
        case '3': save_registers.r3 = value; return 1;
        case '4': save_registers.r4 = value; return 1;
        case '5': save_registers.r5 = value; return 1;
        case '6': save_registers.r6 = value; return 1;
        case '7': save_registers.r7 = value; return 1;
        case '8': save_registers.r8 = value; return 1;
        case '9': save_registers.r9 = value; return 1;
        default: return 0;
      }
    }
    else if (reg[1] == '1') { // r10-r12
      switch(reg[2]) {
        case '0': save_registers.r10 = value; return 1;
        case '1': save_registers.r11 = value; return 1;
        case '2': save_registers.r12 = value; return 1;
        default: return 0;
      }
    }
  }
  else if (strcmp(reg, "lr")==0) save_registers.lr = value;
  else if (strcmp(reg, "pc")==0) save_registers.pc = value;
  else if (strcmp(reg, "sp")==0) save_registers.sp = value;
  else if (strcmp(reg, "cpsr")==0) save_registers.xPSR = value;
  else {
    return 0;
  }
  return 1;
}

/**
 * @brief TODO, flag to restore registers on return
 * 
 * @return int 0 = success
 */
int debug_restoreRunMode() {
  return 0;
}

/**
 * Fault debug messages
 */

/**
 * @brief Blink LED in infinite loop
 * 
 * @param n Code to blink
 */
void flash_blink(int n) {
  volatile int p = 0;
  pinMode(13, OUTPUT);
  while(1) {
    for(int c=0; c<n; c++) {
      for(int i=0; i<20000; i++) {p++;}
      digitalWrite(13, HIGH);
      for(int i=0; i<20000; i++) {p++;}
      digitalWrite(13, LOW);
    }
    for(int i=0; i<100000; i++) {p++;}
  }
}

int debug_crash = 0;

/**
 * @brief Default handler for faults
 * 
 * @param n 
 */
void hard_fault_debug(int n) {
  Serial.print("****FAULT ");
  Serial.println(hard_fault_debug_text[n]);
  Serial.print("r0=");Serial.println(stack->r0, HEX);
  Serial.print("r1=");Serial.println(stack->r1, HEX);
  Serial.print("r2=");Serial.println(stack->r2, HEX);
  Serial.print("r3=");Serial.println(stack->r3, HEX);
  Serial.print("r12=");Serial.println(stack->r12, HEX);
  Serial.print("lr=0x");Serial.println(stack->lr, HEX);
  Serial.print("pc=0x");Serial.println(stack->pc, HEX);
  Serial.println("****************");
  debug_crash = 1;
  stack->pc += 2; // if we continue, skip faulty instruction
  flash_blink(n);
}

extern "C" 
__attribute__((noinline, naked)) 
void fault_halt() {
  while(1) { asm volatile("wfi"); }
}

// uint32_t hard_fault_debug_addr = (uint32_t)hard_fault_debug;

#pragma GCC push_options
#pragma GCC optimize ("O0")

#ifdef DISPLAY_HARD_FAULT

// Save registers during fault and call default handler
#define fault_isr_stack(fault) \
  asm volatile("ldr r0, =stack \n str sp, [r0]"); \
  asm volatile("push {lr}"); \
  hard_fault_debug(fault); \
  asm volatile("pop {pc}")

#else

// Save registers during fault and call special handler that allows debugging
#define fault_isr_stack(fault) \
  asm volatile(SAVE_STACK); \
  asm volatile(SAVE_REGISTERS); \
  asm volatile("push {lr}"); \
  debug_crash = 1; \
  debug_id = fault; \
  debug_call_isr_setup(); \
  asm volatile("pop {pc}")

#endif

/**
 * @brief Trap faults
 * 
 */
__attribute__((noinline, naked)) void call_nmi_isr(void) { fault_isr_stack(2); }
__attribute__((noinline, naked)) void call_hard_fault_isr(void) { fault_isr_stack(3); }
__attribute__((noinline, naked)) void call_memmanage_fault_isr(void) { fault_isr_stack(4); } 
__attribute__((noinline, naked)) void call_bus_fault_isr(void)  { fault_isr_stack(5); }
__attribute__((noinline, naked)) void call_usage_fault_isr(void)  { fault_isr_stack(6); }

#pragma GCC pop_options

/**
 * Initialization code
 * 
 */

/**
 * @brief Utility function to display memory.
 * 
 * @param mem 
 * @param sz 
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

#ifdef __IMXRT1062__
extern "C" void unused_interrupt_vector(void);
#else
extern "C" void unused_isr(void);
#endif

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
  hw_remap_table = (uint16_t *)xtable;
  FP_CTRL = 0b11;

  // delay(3000);
//  Serial.println(FP_CTRL, HEX);
//  dumpmem(sourcemem, 32);
//  dumpmem(destmem, 32);
//  dumpmem(xtable, 32);
#endif

  debug_trace = 1;

  _VectorsRam[2] = call_nmi_isr;
  _VectorsRam[3] = call_hard_fault_isr;
  _VectorsRam[4] = call_memmanage_fault_isr;
  _VectorsRam[5] = call_bus_fault_isr;
  _VectorsRam[6] = call_usage_fault_isr;

#ifdef __IMXRT1062__
  if (_VectorsRam[11] == unused_interrupt_vector) {
#else
  if (_VectorsRam[11] == unused_isr) {
#endif
    original_svc_isr = 0;
  }
  else {
    original_svc_isr = _VectorsRam[11];
  }
  _VectorsRam[11] = svcall_isr;

  // chain the software ISR handler
  original_software_isr = _VectorsRam[IRQ_SOFTWARE + 16];

  _VectorsRam[IRQ_SOFTWARE + 16] = debug_call_isr;
  NVIC_SET_PRIORITY(IRQ_SOFTWARE, 208); // 255 = lowest priority
  NVIC_ENABLE_IRQ(IRQ_SOFTWARE);

  debug_initBreakpoints();
}

void gdb_init(Stream *device);

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

#ifdef REMAP_SETUP

// We will rename the original setup() to this by using a #define
void setup_main();

void debug_setup_auto() {
  // delay(3000);
  // Serial.println("Auto setup debug serial");
  // delay(3000);
  // return;
#if defined(GDB_DUAL_SERIAL)
  debug_begin(&SerialUSB1);
#elif defined(GDB_TAKE_OVER_SERIAL)
  debug_begin(&Serial);
#else
  debug_begin(NULL);
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

  asm volatile("push {r0-r4,lr}");                          // save the return address
  debug_setup_auto();
  setup_main();                                       // call the "real" setup function
  asm volatile("pop {r0-r4,pc}");                           // get original return address

}

#endif

/**
 * Class helper Debug
 */

int Debug::begin(Stream *device) { return debug_begin(device); }
int Debug::setBreakpoint(void *p) { return debug_setBreakpoint(p); }
int Debug::clearBreakpoint(void *p) { return debug_clearBreakpoint(p); }
void Debug::setCallback(void (*c)()) { callback = c; }
uint32_t Debug::getRegister(const char *reg) { return debug_getRegister(reg); }
int Debug::setRegister(const char *reg, uint32_t value) { return debug_setRegister(reg, value); }
// int Debug::restoreRunMode() { return debug_restoreRunMode(); }

Debug debug;
