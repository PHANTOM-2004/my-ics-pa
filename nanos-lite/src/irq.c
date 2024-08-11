#include "am.h"
#include <common.h>
#include "proc.h"

extern void do_syscall(Context *c);
extern Context *schedule(Context *prev) ;

static Context *do_event(Event e, Context *c) {
  switch (e.event) {
  case EVENT_YIELD:
    // printf("Handle EVENT_YIELD\n");
    c = schedule(c);
    break;
  case EVENT_SYSCALL:
    // printf("Handle EVENT_SYSCALL\n");
    do_syscall(c);
    break;
  default:
    panic("Unhandled event ID = %d", e.event);
  }
  //printf("ret[a0]:%d\n", c->GPRx);
  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
