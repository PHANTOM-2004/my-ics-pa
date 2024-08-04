#include "trap.h"

char buf[128];

#define Log(format, ...) \
  printf("\33[1;35m[%s,%d,%s] " format "\33[0m\n", \
      __FILE__, __LINE__, __func__, ## __VA_ARGS__)

int main() {
  printf("%s", "Hello world!\n");

  printf("%d + %d = %d\n", 1, 1, 2);

  printf("%d + %d = %d\n", 2, 10, 12);
  
  printf("Unhandled syscall ID = %d\n", 0);
  return 0;
}
