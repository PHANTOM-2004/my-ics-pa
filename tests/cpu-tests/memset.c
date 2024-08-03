#include "trap.h"

#define N 96 // max 255
uint8_t data[N];

#define NO_OUTPUT

void reset() {
  int i;
  for (i = 0; i < N; i++) {
    data[i] = (uint8_t)(i + 1);
  }
}

// 检查[l,r)区间中的值是否依次为val, val + 1, val + 2...
void check_seq(int l, int r) {
  int i;
  for (i = l; i < r; i++) {
    assert(data[i] == (uint8_t)(i + 1));
  }
}

// 检查[l,r)区间中的值是否均为val
void check_eq(int l, int r, uint8_t val) {
  int i;
  for (i = l; i < r; i++) {
    assert(data[i] == val);
  }
}

void test_memset() {
  int l, r;
  for (l = 0; l < N; l++) {
    for (r = l + 1; r <= N; r++) {
      reset();
      uint8_t val = (l + r) / 2;
#ifndef NO_OUTPUT
      printf("[%d, %d)=%u\n", l, r, val);
      for (int i = 0; i < N; i++)
        printf("%u %s", data[i], i == N - 1 ? "\n" : "");

#endif /* ifndef  NO_OUTPUT */
      memset(data + l, val, r - l);

#ifndef NO_OUTPUT
      for (int i = 0; i < N; i++)
        printf("%u %s", data[i], i == N - 1 ? "\n" : "");
#endif
      check_seq(0, l);
      check_eq(l, r, val);
      check_seq(r, N);
    }
  }
}

int main() { test_memset(); }
