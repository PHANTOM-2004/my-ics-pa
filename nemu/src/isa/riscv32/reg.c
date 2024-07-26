/***************************************************************************************
 * Copyright (c) 2014-2022 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include "local-include/reg.h"
#include "common.h"
#include <isa.h>
#include <stdint.h>
#include <sys/types.h>

const char *regs[] = {"$0", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                      "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                      "a6", "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                      "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

static void print_byte(uint8_t const _byte) {
  for (int i = 7; i >= 0; i--) {
    word_t const mask = 1 << i;
    printf("%u", !!(_byte & mask));
  }
}
void isa_reg_display() {
  /* this is what gdb print
  (gdb) info registers
rax            0x555555555159      93824992235865
rbx            0x7fffffffda18      140737488345624
rcx            0x555555557dc0      93824992247232
rdx            0x7fffffffda28      140737488345640
rsi            0x7fffffffda18      140737488345624
rdi            0x1                 1
rbp            0x7fffffffd900      0x7fffffffd900
rsp            0x7fffffffd900      0x7fffffffd900
r8             0x7ffff7e18080      140737352138880
r9             0x7ffff7e0be78      140737352089208
r10            0x7ffff7fcb878      140737353922680
r11            0x7ffff7fe1930      140737354012976
r12            0x0                 0
r13            0x7fffffffda28      140737488345640
r14            0x555555557dc0      93824992247232
r15            0x7ffff7ffd020      140737354125344
rip            0x55555555515d      0x55555555515d <main+4>
eflags         0x246               [ PF ZF IF ]
cs             0x33                51
ss             0x2b                43
ds             0x0                 0
es             0x0                 0
fs             0x0                 0
gs             0x0                 0
  */
  // 32 register total
  for (int i = 0; i < 32; i++) {
    // we use word_t here
    // 32/8 = 4
    word_t const cur_reg = gpr(i);
    printf("R%02d\t %-5s\t HEX: " FMT_WORD, i, reg_name(i), cur_reg);
    // TODO: we need to use bit rather than pointer
    printf("\tBINARY: ");
    for (int t = 3; t >= 0; t--) {
      uint8_t const cur = (uint8_t)BITS(cur_reg, t * 8 + 7, t * 8);
      print_byte(cur);
      printf(" ");
    }
    printf("\n");
 }
}

word_t isa_reg_str2val(const char *s, bool *success) {
  for (int i = 0; i < 32; i++) {
    int const ret = strcmp(s, reg_name(i));
    if (ret)
      continue;
    *success = true;
    Log("convert register %s -> %d", s, i);
    return gpr(i);
  }

  Log("[REGISTER] unknown register [%s]", s);
  *success = false;
  return 0;
}
