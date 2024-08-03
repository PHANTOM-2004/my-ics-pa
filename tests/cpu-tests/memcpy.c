#include "trap.h"

#define NO_OUTPUT
#define N 128
uint8_t data[N];
uint8_t buffer[N];

void gen_buffer(int const l, int const r){
  for(int i = l; i < r;i++){
    buffer[i] = rand();
  }
}

void reset() {
  int i;
  for (i = 0; i < N; i++) {
    data[i] = (uint8_t)(i + 1);
  }
}

void check_seq(int l, int r) {
  int i;
  for (i = l; i < r; i++) {
    assert(data[i] == (uint8_t)(i + 1));
  }
}

void check_eq(int l, int r) {
  int i;
  for (i = l; i < r; i++) {
    assert(data[i] == buffer[i]);
  }
}




void test_memcpy() {
  int l, r;
  for (l = 0; l < N; l++) {
    for (r = l + 1; r <= N; r++) {
      reset();
      gen_buffer(l, r);
#ifndef NO_OUTPUT
      for (int i = 0; i < N; i++)
        printf("%u %s", data[i], i == N - 1 ? "\n" : "");
#endif /* ifndef  NO_OUTPUT */
      memcpy(data + l,buffer + l, r - l);
      //memset(data + l, val, r - l);

#ifndef NO_OUTPUT
      for (int i = 0; i < N; i++)
        printf("%u %s", data[i], i == N - 1 ? "\n" : "");
#endif
      check_seq(0, l);
      check_eq(l, r);
      check_seq(r, N);
    }
  }
}

int main(){
  test_memcpy();
}
