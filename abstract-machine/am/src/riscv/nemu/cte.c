#include <am.h>
#include <klib.h>
#include <stdint.h>

static Context *(*user_handler)(Event, Context *) = NULL;

Context *__am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};

    switch (c->mcause) {
    // machine mode(highest)
    case 0xb: {
      // judege the system call
      switch (c->GPR1) {
      case (uint32_t)-1:
        ev.event = EVENT_YIELD;
        break;
      default: // unknown type
        ev.event = EVENT_SYSCALL;
        break;
      }
      // mepc += 4;
      c->mepc += 4; // add 4 in software
    } break;
    // unknown case
    default:
      ev.event = EVENT_ERROR;
      break;
    }

    c = user_handler(ev, c);
    assert(c != NULL);
  }
  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context *(*handler)(Event, Context *)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context *kcontext(Area kstack, void (*entry)(void *), void *arg) {
  /*

  |               |
  +---------------+ <---- kstack.end
  |               |
  |    context    |
  |               |
  +---------------+ <--+
  |               |    |
  |               |    |
  |               |    |
  |               |    |
  +---------------+    |
  |       cp      | ---+
  +---------------+ <---- kstack.start
  |               |

  */
  // construct a context at the bottom of stack, then return a pointer

  Context *const ret = (Context*)kstack.end - 1;
  ret->mepc = (uintptr_t)entry; // set the entry point
  // in order to pass difftest
  ret->mstatus = 0x1800;
  ret->gpr[10] = (uintptr_t)arg; // args pass by a0
  return ret;
}

void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall");
#endif
}

bool ienabled() { return false; }

void iset(bool enable) {}
