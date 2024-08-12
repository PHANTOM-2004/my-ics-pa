#include "klib-macros.h"
#include "memory.h"
#include <fs.h>
#include <proc.h>

#define MAX_NR_PROC 4

extern void naive_uload(PCB *pcb, const char *filename);

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() { current = &pcb_boot; }

void hello_fun(void *arg) {
  int j = 1;
  while (1) {
    if (j % 10 == 0)
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
  Log("kstack [0x%x, 0x%x)", r.start, r.end);
  Log("initial[%s] cp[0x%x]", "hello_fun", pcb->cp);
}

void context_uload(PCB *const pcb, char const *const fname, char const *argv[],
                   char *const envp[]) {
  // NOTE: use new_page to set user stack
  Log("Load user [%s]", fname);

  int argc = 0, envpc = 0;

  Area const kstack_area = {
      .start = (void *)pcb,
      .end = (void *)((uintptr_t)pcb + (uintptr_t)sizeof(pcb->stack))};

  Log("kstack [0x%x, 0x%x)", kstack_area.start, kstack_area.end);

  void *const ustk_begin = new_page(STACK_SIZE / PGSIZE);
  Area const ustack_area = {.start = ustk_begin,
                            .end = ustk_begin + STACK_SIZE};

  uintptr_t const string_area_start = ((uintptr_t)ustack_area.end - 64);
  uintptr_t sptr = string_area_start;

  // first copy argv string
  Log("argv %x", argv ? argv : 0);
  assert(argv);
  Log("argv[0] %x", argv[0]);
  for (char const **p = argv; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    argc++;
    Log("argv [%s] copied", *p);
  }
  Log("copied argv strings, [%d] string(s)", argc);

  // then copy envp string
  assert(envp);
  for (char *const *p = envp; *p != NULL; p++) {
    size_t const len = strlen(*p) + 1;
    sptr -= len;
    memcpy((void *)sptr, *p, len);
    envpc++;
    Log("envp [%s] copied", *p);
  }
  Log("copied envp strings, [%d] string(s)", envpc);
  Log("string area[%x,%x]", string_area_start, sptr);

  sptr -= 64;                                // let the unspecified = 64;
  sptr = ROUNDDOWN(sptr, sizeof(uintptr_t)); // align with pointer

  // copy envp pointer, first is NULL
  assert(envp[envpc] == NULL);
  for (int i = envpc; i >= 0; i--) {
    sptr -= sizeof(char *);
    char **pos = (void *)sptr;
    *pos = envp[i];
    Log("ENV(%d)[0x%x] %s", i, pos, !*pos ? "NULL" : *pos);
  }
  Log("copied envp pointers, [%d] pointer(s)", envpc);

  // copy argv pointer, first is NULL
  assert(argv[argc] == NULL);
  for (int i = argc; i >= 0; i--) {
    sptr -= sizeof(char const *);
    char const **pos = (void *)sptr;
    *pos = argv[i];
    Log("ARG(%d)[0x%x] %s", i, pos, !*pos ? "NULL" : *pos);
  }
  Log("copied argv, [%d] pointer(s)", argc);

  // finally copy argc
  sptr -= sizeof(int);
  *(int *)sptr = argc;
  Log("ARGC:%d", *(int *)sptr);
  Log("TOTAL copied [%x, %x]", sptr, string_area_start);

  // load the user program
  extern uintptr_t loader(PCB * pcb, const char *filename);
  uintptr_t const entry = loader(pcb, fname); // get_elf_entry(fname);

  pcb->cp = ucontext(NULL, kstack_area, (void *)entry);
  Log("initial[%s] cp[0x%x]", fname, pcb->cp);
  // then we need to load the strings to user stack
  // mark string store area

  // set the top of user stack
  pcb->cp->GPRx = (uintptr_t)sptr;
  Log("sp at 0x%x", sptr);
  Log("argc : %x", *(int *)sptr);
}

void init_proc() {
  static char const *argv[] = {"/bin/nterm", NULL};
  static char *const envp[] = {"AM_HOME=/bin/2004", NULL};

  context_kload(&pcb[0], hello_fun, (void *)0xcc);
  // context_kload(&pcb[1], hello_fun, (void *)0xff);
  context_uload(&pcb[1], "/bin/nterm", argv, envp);
  switch_boot_pcb();

  Log("heap.start: [0x%x]  heap.end: [0x%x]", (uintptr_t)heap.start,
      (uintptr_t)heap.end);

  Log("Initializing processes...");
  // load program here
  // naive_uload(pcb, "/bin/nterm");
}

Context *schedule(Context *prev) {
  // Log("schedule PCB, context pointer [0x%x]", prev);
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
