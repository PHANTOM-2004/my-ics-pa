#ifndef ARCH_H__
#define ARCH_H__

#ifdef __riscv_e
#define NR_REGS 16
#else
#define NR_REGS 32
#endif
#include <stdint.h>
struct Context {
  union {
    uintptr_t gpr[NR_REGS];
    void *pdir; // share with gpr[0]
  };
  uintptr_t mcause;
  uintptr_t mstatus;
  uintptr_t mepc;
};

#ifdef __riscv_e
#define GPR1 gpr[15] // a5
#else
#define GPR1 gpr[17] // a7
#endif

#define GPR2 gpr[10] // a0
#define GPR3 gpr[11] // a1
#define GPR4 gpr[12] // a2
#define GPRx gpr[10] // a0

#endif
