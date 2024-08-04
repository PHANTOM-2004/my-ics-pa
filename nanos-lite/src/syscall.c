#include "syscall.h"
#include <common.h>

int sys_yield() {
  yield();
  return 0;
}

int sys_exit(int const status){
  // status is passed by a0
  halt(status);
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2; //it is  a0

  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
