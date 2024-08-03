#include "trap.h"

char buf[128];

int main() {
  printf("%s", "Hello world!\n");

  printf("%d + %d = %d\n", 1, 1, 2);

  printf("%d + %d = %d\n", 2, 10, 12);

  return 0;
}
