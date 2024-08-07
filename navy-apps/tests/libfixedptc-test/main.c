#include <fixedptc.h>
#include <stdio.h>

fixedpt arr[] = {
    fixedpt_rconst(12),         fixedpt_rconst(12.99),
    fixedpt_rconst(-12982.19),  fixedpt_rconst(-298.13),
    fixedpt_rconst(-9999.99),   fixedpt_rconst(-2838),
    fixedpt_rconst(1234.99),    fixedpt_rconst(29143.00),
    fixedpt_rconst(123478.278), fixedpt_rconst(5201314.1314),
    fixedpt_rconst(-0.0),       fixedpt_rconst(+0.0),
};
static int const arr_size = sizeof(arr) / sizeof(arr[0]);
int main() {
  printf("size: %d\n", arr_size);
  for (int i = 0; i < arr_size; i++) {
    fixedpt const _abs = fixedpt_abs(arr[i]);
    fixedpt const _floor = fixedpt_floor(arr[i]);
    fixedpt const _ceil = fixedpt_ceil(arr[i]);
    printf("[%2d] abs: %d floor: %d ceil: %d\n", i, fixedpt_toint(_abs),
           fixedpt_toint(_floor), fixedpt_toint(_ceil));
  }
  return 0;
}
