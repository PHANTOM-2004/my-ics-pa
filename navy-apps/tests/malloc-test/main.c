#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#define N 10

int *a[N];

int main() {
  write(1, "Hello World!\n", 13);

  for (int i = 0; i < N; i++){
    a[i] = (int *)malloc(sizeof(int) * i);
    if(!a[i]) putchar(i + '0');
    assert(a[i]);
  }

  for (int i = 0; i < N; i++)
    free(a[i]);

  return 0;
}
