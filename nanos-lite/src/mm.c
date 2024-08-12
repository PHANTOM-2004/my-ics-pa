#include <memory.h>

static void *pf = NULL;

void *new_page(size_t nr_page) {
  void *const res = pf;
  pf = (void *)((uintptr_t)pf + (uintptr_t)nr_page * PGSIZE);

  assert((uintptr_t)pf <= (uintptr_t)heap.end);
  Log("allocate pages: [%x, %x)", (uintptr_t)res, (uintptr_t)pf);

  return res;
}

#ifdef HAS_VME
static void *pg_alloc(int n) { return NULL; }
#endif

void free_page(void *p) { panic("not implement yet"); }

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) { return 0; }

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}
