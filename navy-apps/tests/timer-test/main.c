#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

int main() {
  struct timeval tv[1];
  uint64_t last = 0;
  uint64_t current = 0;
  int t = 100;
  while (t) {
    gettimeofday(tv, NULL);
    current = (tv->tv_sec * 1000000 + tv->tv_usec) / 1000; // get ms
    if (current - last >= 500) {
      t--;
      printf("TIME: %d\n", (int)current);
      last = current;
    }
  }
  return 0;
}
