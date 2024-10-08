#include "syscall.h"
#include "amdev.h"
#include "fs.h"
#include "log.h"
#include "proc.h"
#include <sys/time.h>

static inline int sys_yield() {
  yield();
  return 0;
}

static int sys_execve(const char *fname, char const *argv[],
                      char *const envp[]) {
  /* On success, execve() does not return, on error -1 is returned,
   * and errno is set to indicate the error.*/
  // we only consider fname here

  // now call context_uload
  Log("call sys_execve");
  Log("Switching to app: [%s], argv: [0x%x](arg0:[0x%x]), envp: [0x%x]", fname,
      argv, argv[0], envp);

  int ret = fs_open(fname, 0, 0);
  if (ret < 0) { // the file does not exist
    return ret;  // return -errno, here it is -2
  } else
    fs_close(ret);

  context_uload(current, fname, argv, envp);
  switch_boot_pcb();
  yield();

  assert(0);
  return -1;
}

static inline int sys_exit(int const status) {
  // call /bin/menu
  static char const *argv[] = {"/bin/nterm", NULL};
  static char *const envp[] = {NULL};
  sys_execve("/bin/nterm", argv, envp);
  assert(0);
  // status is passed by a0
  halt(status);
}

static inline size_t sys_write(int fd, void const *buf, size_t count) {
  return fs_write(fd, buf, count);
}

static inline int sys_brk(intptr_t const increment) { return 0; }

static inline int sys_open(const char *pathname, int flags, int mode) {
  return fs_open(pathname, flags, mode); //  fails: -2
}

static inline int sys_close(int fd) { return fs_close(fd); }

static inline size_t sys_lseek(int fd, size_t offset, int whence) {
  return fs_lseek(fd, offset, whence);
}

static inline size_t sys_read(int fd, void *buf, size_t len) {
  return fs_read(fd, buf, len);
}
static int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
  // gettimeofday() and settimeofday() return 0 for success.
  // On error, -1 is returned and errno is set to indicate the error.
  int res = -1;
  if (tv) {
    uint64_t const us =
        io_read(AM_TIMER_UPTIME).us; // we can also use UNIX EPOCH
    tv->tv_sec = us / 1000000;
    tv->tv_usec = us % 1000000;
    res = 0;
  }

  if (tz) {
    tz->tz_dsttime = 0;
    tz->tz_minuteswest = 0;
    res = 0;
  }
  return res;
}

void do_syscall(Context *c) {
  uintptr_t a[4] = {c->GPR1, c->GPR2, c->GPR3, c->GPR4};

#ifdef CONFIG_STRACE
  strace(a[0], a[1], a[2], a[3], c->mepc);
#endif

  switch (a[0]) {
  case SYS_yield:
    sys_yield();
    break;
  case SYS_exit:
    sys_exit(a[1]);
    break;
  case SYS_write:
    c->GPRx = (uintptr_t)sys_write(a[1], (void const *)a[2], a[3]);
    break;
  case SYS_brk:
    c->GPRx = (uintptr_t)sys_brk(a[1]);
    break;
  case SYS_read:
    c->GPRx = (uintptr_t)sys_read(a[1], (void *)a[2], a[3]);
    break;
  case SYS_open:
    c->GPRx = (uintptr_t)sys_open((char const *)a[1], a[2], a[3]);
    break;
  case SYS_close:
    c->GPRx = (uintptr_t)sys_close(a[1]);
    break;
  case SYS_lseek:
    c->GPRx = (uintptr_t)sys_lseek(a[1], a[2], a[3]);
    break;
  case SYS_gettimeofday:
    c->GPRx = (uintptr_t)sys_gettimeofday((struct timeval *)a[1],
                                          (struct timezone *)a[2]);
    break;
  case SYS_execve:
    c->GPRx = (uintptr_t)sys_execve(
        (char const *)a[1], (char const **)a[2],
        (char *const *)a[3]); // file not found will return -1
    break;
  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}
