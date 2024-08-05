#include "common.h"
#include <stdint.h>

#ifdef CONFIG_STRACE
#include "log.h"
#include "syscall.h"

#define CONFIG_STRACE_SIZE 8192
// #define CONFIG_SYSWRITE_TRACE_OUT

typedef struct {
  uintptr_t event_type;
  uintptr_t a0;
  uintptr_t a1;
  uintptr_t a2;
  uintptr_t epc;
} strace_stk;

static int top = 0;
static strace_stk strace_events[CONFIG_STRACE_SIZE];
static bool stk_full = false;

static void print_single_syscall(int const i) {
  strace_stk const p = strace_events[i];
#ifndef CONFIG_SYSWRITE_TRACE_OUT
  if (SYS_write != p.event_type)
#endif
    printf("%sEPC[0x%x]: ", ANSI_FMT("[STRACE]", ANSI_FG_MAGENTA), p.epc);

  switch (p.event_type) {
  case SYS_yield:
    printf("sys_yield\n");
    break;
  case SYS_exit:
    printf("sys_exit[%d]\n", p.a0);
    break;
  case SYS_write:
#ifdef CONFIG_SYSWRITE_TRACE_OUT
    printf("sys_write[fd=%d buf=%x count=%u\n", p.a0, p.a1, p.a2);
#endif
    break;
  case SYS_brk:
    printf("sys_brk[increment=%d]\n", p.a0);
    break;
  case SYS_read:
    printf("sys_read[fd=%d buf=%x count=%u]\n", p.a0, p.a1, p.a2);
    break;
  case SYS_lseek:
    printf("sys_lseek[fd=%d offset=%x whence=%d]\n", p.a0, p.a1, p.a2);
    break;
  case SYS_open:
    printf("sys_open[pathname=%s]\n", p.a0);
    break;
  case SYS_close:
    printf("sys_close[fd=%d]\n", p.a0);
    break;
  case SYS_gettimeofday:
    //printf("sys_gettimeofday[&tv=%x &tz=%x]\n", p.a0, p.a1);
    printf("--get tv [%d]\n", *(int*)p.a0);
    break;
  default:
    panic("Unknown syscall ID = %d", p.event_type);
  }
}

int strace(uintptr_t const ev_type, uintptr_t const a0, uintptr_t const a1,
           uintptr_t const a2, uintptr_t const epc) {
  strace_events[top] = (strace_stk){
      .event_type = ev_type, .a0 = a0, .a1 = a1, .a2 = a2, .epc = epc};

  print_single_syscall(top);

  top++;
  if (top == CONFIG_STRACE_SIZE)
    top = 0, stk_full = true;
  return 0;
}

void print_strace() {
  int const plen = stk_full ? CONFIG_STRACE_SIZE : top;
  for (int i = 0; i < plen; i++) {
    printf("  %s", (i + 1 == top) || (!top && stk_full) ? "--->\t" : "  \t");
    print_single_syscall(i);
  }
}

#endif
