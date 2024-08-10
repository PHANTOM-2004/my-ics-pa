#include <assert.h>
#include <unistd.h>

int main() {

  int t = 10;
  while (t--) {
    void *i = sbrk(1000);
    assert(i != (void *)-1);
  }

  return 0;
}
