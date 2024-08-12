#include "elfp.h"
#include <fs.h>
#include <proc.h>
#include <stdint.h>

#define MAX_NR_PROC 4

extern void naive_uload(PCB *pcb, const char *filename);

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
  //  current = &pcb[0];
}

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!",
        (uintptr_t)arg, j);
    j++;
    yield();
  }
}

void context_kload(PCB *const pcb, void (*entry)(void *), void *const arg) {
  Area const r = {.start = (void *)pcb,
                  .end =
                      (void *)((uintptr_t)pcb + (uintptr_t)sizeof(pcb->stack))};
  pcb->cp = kcontext(r, entry, arg);
}

void context_uload(PCB *const pcb, char const *const fname, char const *argv[],
                   char *const envp[]) {
  // NOTE:heap.end is the stack top for user program

  Area const kstack_area = {
      .start = (void *)pcb,
      .end = (void *)((uintptr_t)pcb + (uintptr_t)sizeof(pcb->stack))};

  extern uintptr_t loader(PCB * pcb, const char *filename);
  uintptr_t const entry = loader(pcb, fname); // get_elf_entry(fname);

  pcb->cp = ucontext(NULL, kstack_area, (void *)entry);

  // then we need to load the strings to user stack
  uintptr_t const string_area_start = ((uintptr_t)kstack_area.end - 64);
  uintptr_t sptr = string_area_start;

  int argc = 0, envpc = 0;

  // first copy argv string
  for (char const **p = argv; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    argc++;
  }
  Log("copied argv string, [%d] strings", argc);

  // then copy envp string
  for (char *const *p = envp; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    envpc++;
  }
  Log("copied envp string, [%d] strings", envpc);

  sptr -= 64;                             // let the unspecified = 64;
  sptr = sptr & (~sizeof(uintptr_t) + 1); // align with pointer

  // copy envp pointer, first is NULL
  assert(envp[envpc] == NULL);
  for (int i = envpc; i >= 0; i--) {
    sptr -= sizeof(char *);
    char **pos = (void *)sptr;
    *pos = envp[i];
  }
  Log("copied envp, [%d] pointer", envpc);

  // copy argv pointer, first is NULL
  assert(argv[argc] == NULL);
  for (int i = argc; i >= 0; i--) {
    sptr -= sizeof(char const *);
    char const **pos = (void *)sptr;
    *pos = argv[i];
  }
  Log("copied argv, [%d] pointer", argc);

  // finally copy argc
  sptr -= sizeof(int);
  *(int *)sptr = argc;

  // in amd64, when set sptr
  // (rsp + 8) should be multiple of 16 when
  // control is transfered to the function entry point.
  // See amd64 ABI manual for more details

  // set the top of user stack
  pcb->cp->GPRx = (uintptr_t)sptr;
  Log("sp at 0x%x", sptr);
}

void init_proc() {
  static char const *argv[] = {"--skip", "abc", NULL};
  static char *const envp[] = {"AM_HOME=/bin/2004", NULL};

  context_kload(&pcb[0], hello_fun, (void *)0xcc);
  // context_kload(&pcb[1], hello_fun, (void *)0xff);
  context_uload(&pcb[1], "/bin/bird", argv, envp);

  switch_boot_pcb();
  Log("heap.start: [0x%x]  heap.end(GPRx->sp): [0x%x]", (uintptr_t)heap.start,
      (uintptr_t)heap.end);
  Log("Initializing processes...");
  // load program here
  // naive_uload(pcb, "/bin/malloc-test");
}

Context *schedule(Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
