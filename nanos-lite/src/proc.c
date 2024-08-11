#include <proc.h>

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

void init_proc() {
  context_kload(&pcb[0], hello_fun, (void *)0xcc);
  context_kload(&pcb[1], hello_fun, (void *)0xff);

  switch_boot_pcb();

  yield(); // add yield here

  Log("Initializing processes...");

  // load program here
  naive_uload(pcb, "/bin/malloc-test");
}

Context *schedule(Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
  return current->cp;
}
