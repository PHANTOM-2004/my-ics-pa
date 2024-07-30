#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
static unsigned long int next = 1;

int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) { next = seed; }

int abs(int x) { return (x < 0 ? -x : x); }

int atoi(const char *nptr) {
  int x = 0;
  while (*nptr == ' ') {
    nptr++;
  }
  while (*nptr >= '0' && *nptr <= '9') {
    x = x * 10 + *nptr - '0';
    nptr++;
  }
  return x;
}

void *malloc(size_t size) {
  // On native, malloc() will be called during initializaion of C runtime.
  // Therefore do not call panic() here, else it will yield a dead recursion:
  //   panic() -> putchar() -> (glibc) -> malloc() -> panic()

  /*       The malloc(), calloc(), realloc(), and reallocarray() functions
   *       return a pointer to the allocated  memory,  which  is
       suitably  aligned  for any type that fits into the requested size or
   less.

       On error, these functions return NULL and
       set errno.  Attempting to allocate more than PTRDIFF_MAX bytes is
   considered an error, as an object that large  could cause later pointer
   subtraction to overflow.
*/
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
  static uint8_t *addr = NULL;
  if (addr == NULL)
    addr = heap.start;
  size = ROUNDUP(size, 8);
  void *const res = (void *)addr;
  addr += size;
  assert((uint32_t)addr >= (uint32_t)heap.start && (uint32_t)addr < (uint32_t)heap.end);
  for (uint32_t *p = res; p != (uint32_t*)addr; p++)
    *p = 0; // actually no need to set zero

  return res;
#endif
  return NULL;
}

void free(void *ptr) {}

#endif
