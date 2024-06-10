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

const char *regs[] = {"$0", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
                      "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
                      "a6", "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
                      "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

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
    printf("RegID: %2d RegName: %-10s Content: " FMT_WORD, i, regs[i], cur_reg);
    void const *ptr = &cur_reg;
    word_t const R3 = *(uint8_t *)(ptr);
    word_t const R2 = *(uint8_t *)(ptr + 1);
    word_t const R1 = *(uint8_t *)(ptr + 2);
    word_t const R0 = *(uint8_t *)(ptr + 3);
    printf("\t\tDetailed: %08u %08u %08u %08u\n", R3, R2, R1, R0);
  }
}

word_t isa_reg_str2val(const char *s, bool *success) { return 0; }
