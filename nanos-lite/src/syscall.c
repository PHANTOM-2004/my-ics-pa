#include "syscall.h"
#include "log.h"
#include <common.h>

static int sys_yield() {
  yield();
  return 0;
}

static int sys_exit(int const status) {
  // status is passed by a0
  halt(status);
}

static int sys_write(int fd, void const *buf, size_t count) {
  // sys write need to ret correct value;
  // On success, the number of bytes written is returned.
  // On error, -1 is returned, and errno is set to indicate the error.
  int ret = 0;
  assert(fd == 1 || fd == 2);
  for (size_t i = 0; i < count; i++) {
    putch(((char const *)buf)[i]);
    ret++;
  }
  return ret;
}

static int sys_brk(intptr_t const increment) { return 0; }

void do_syscall(Context *c) {
  uintptr_t a[4] = {c->GPR1, c->GPR2, c->GPR3, c->GPR4};
#ifdef CONFIG_STRACE
  strace(a[0], a[1], a[2], a[3]);
#endif
  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  case SYS_write:
    c->GPRx = sys_write(a[1], (void const *)a[2], a[3]);
    break;
  case SYS_brk:
    c->GPRx = sys_brk(a[1]);
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
