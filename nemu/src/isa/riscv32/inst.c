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

#include "common.h"
#include "isa.h"
#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/ifetch.h>

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
  TYPE_I,
  TYPE_U,
  TYPE_S,
  TYPE_N, // none
  TYPE_B,
  TYPE_J,
  TYPE_R,
  TYPE_CSR,
};

#define src1R()                                                                \
  do {                                                                         \
    *src1 = R(rs1);                                                            \
  } while (0)
#define src2R()                                                                \
  do {                                                                         \
    *src2 = R(rs2);                                                            \
  } while (0)
#define srcCSR()                                                               \
  do {                                                                         \
    *_csr = csr(*_csr_addr);                                                   \
  } while (0)
#define immI()                                                                 \
  do {                                                                         \
    *imm = SEXT(BITS(i, 31, 20), 12);                                          \
  } while (0)
#define immU()                                                                 \
  do {                                                                         \
    *imm = SEXT(BITS(i, 31, 12), 20) << 12;                                    \
  } while (0)
#define immS()                                                                 \
  do {                                                                         \
    *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7);                   \
  } while (0)
#define immB()                                                                 \
  do {                                                                         \
    *imm = SEXT((BITS(i, 31, 31) << 12) | (BITS(i, 7, 7) << 11) |              \
                    (BITS(i, 30, 25) << 5) | (BITS(i, 11, 8) << 1),            \
                13);                                                           \
  } while (0)
#define immJ()                                                                 \
  do {                                                                         \
    *imm = SEXT((BITS(i, 31, 31) << 20) | (BITS(i, 19, 12) << 12) |            \
                    (BITS(i, 20, 20) << 11) | (BITS(i, 30, 21) << 1),          \
                21);                                                           \
  } while (0)

static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2,
                           int *_csr_addr, word_t *_csr, word_t *imm,
                           int type) {
  word_t const i = s->isa.inst.val;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *_csr_addr = BITS(i, 31, 20); // csr address
  *rd = BITS(i, 11, 7);
  switch (type) {
  case TYPE_I:
    src1R();
    immI();
    break;
  case TYPE_U:
    immU();
    break;
  case TYPE_J:
    immJ();
    break;
  case TYPE_S:
    src1R();
    src2R();
    immS();
    break;
  case TYPE_B:
    src1R();
    src2R();
    immB();
    break;
  case TYPE_R:
    src1R();
    src2R();
    break;
  case TYPE_CSR:
    src1R();
    if (*rd == 0)
      break;
    srcCSR();
    break;
  case TYPE_N:
    break;
  default:
    Log("unknown type %d", type);
  }
}

static int decode_exec(Decode *s) {
  int rd = 0, _csr_addr = 0;
  word_t src1 = 0, src2 = 0, imm = 0, _csr = 0;
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst.val)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */)                   \
  {                                                                            \
    decode_operand(s, &rd, &src1, &src2, &_csr_addr, &_csr, &imm,              \
                   concat(TYPE_, type));                                       \
    __VA_ARGS__;                                                               \
  }

  INSTPAT_START();
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc, U,
          R(rd) = s->pc + imm);

  /*instructions for loading and storing*/
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb, S,
          Mw(src1 + imm, 1, src2));
  INSTPAT("??????? ????? ????? 001 ????? 01000 11", sh, S,
          Mw(src1 + imm, 2, src2));
  INSTPAT("??????? ????? ????? 010 ????? 01000 11", sw, S,
          Mw(src1 + imm, 4, src2));
  INSTPAT("??????? ????? ????? 010 ????? 00000 11", lw, I,
          R(rd) = Mr(src1 + imm, 4));
  INSTPAT("??????? ????? ????? 001 ????? 00000 11", lh, I,
          R(rd) = SEXT(Mr(src1 + imm, 2), 16));
  INSTPAT("??????? ????? ????? 000 ????? 00000 11", lb, I,
          R(rd) = SEXT(Mr(src1 + imm, 1), 8));
  INSTPAT("??????? ????? ????? 101 ????? 00000 11", lhu, I,
          R(rd) = Mr(src1 + imm, 2));
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu, I,
          R(rd) = Mr(src1 + imm, 1));

  /*instructions with immediate */
  INSTPAT("??????? ????? ????? 000 ????? 00100 11", addi, I,
          R(rd) = imm + src1); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? 010 ????? 00100 11", slti, I,
          R(rd) = (sword_t)src1 < (sword_t)imm); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? 011 ????? 00100 11", sltiu, I,
          R(rd) = src1 < imm); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? 100 ????? 00100 11", xori, I,
          R(rd) = imm ^ src1); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? 110 ????? 00100 11", ori, I,
          R(rd) = imm | src1); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? 111 ????? 00100 11", andi, I,
          R(rd) = imm & src1); // rs1 + imm -> rd
  INSTPAT("0000000 ????? ????? 001 ????? 00100 11", slli, I,
          R(rd) = src1 << BITS(imm, 4, 0)); // rs1 + imm -> rd
  INSTPAT("0000000 ????? ????? 101 ????? 00100 11", srli, I,
          R(rd) = src1 >> BITS(imm, 4, 0)); // rs1 + imm -> rd
  INSTPAT("0100000 ????? ????? 101 ????? 00100 11", srai, I,
          R(rd) =
              (word_t)((sword_t)src1 >> BITS(imm, 4, 0))); // rs1 + imm -> rd
  INSTPAT("??????? ????? ????? ??? ????? 01101 11", lui, U, R(rd) = imm);

  /*instructions for alrithmetic*/
  INSTPAT("0000000 ????? ????? 000 ????? 01100 11", add, R,
          R(rd) = src1 + src2);
  INSTPAT("0100000 ????? ????? 000 ????? 01100 11", sub, R,
          R(rd) = src1 - src2);
  INSTPAT("0000000 ????? ????? 001 ????? 01100 11", sll, R,
          R(rd) = src1 << BITS(src2, 4, 0));
  INSTPAT("0000000 ????? ????? 010 ????? 01100 11", slt, R,
          R(rd) = (sword_t)src1 < (sword_t)src2);
  INSTPAT("0000000 ????? ????? 011 ????? 01100 11", sltu, R,
          R(rd) = src1 < src2);
  INSTPAT("0000000 ????? ????? 100 ????? 01100 11", xor, R,
          R(rd) = src1 ^ src2);
  INSTPAT("0000000 ????? ????? 101 ????? 01100 11", srl, R,
          R(rd) = src1 >> BITS(src2, 4, 0));
  INSTPAT("0100000 ????? ????? 101 ????? 01100 11", sra, R,
          R(rd) = (word_t)((sword_t)src1 >> BITS(src2, 4, 0)));
  INSTPAT("0000000 ????? ????? 110 ????? 01100 11", or, R, R(rd) = src1 | src2);
  INSTPAT("0000000 ????? ????? 111 ????? 01100 11", and, R,
          R(rd) = src1 & src2);

  /* extended instructions */
  INSTPAT("0000001 ????? ????? 000 ????? 0110011", mul, R,
          R(rd) = (word_t)((sword_t)src1 * (sword_t)src2));
  INSTPAT("0000001 ????? ????? 001 ????? 0110011", mulh, R,
          R(rd) = (word_t)BITS((int64_t)(sword_t)src1 * (int64_t)(sword_t)src2,
                               63, 32));
  INSTPAT("0000001 ????? ????? 010 ????? 0110011", mulhsu, R,
          R(rd) =
              (word_t)BITS((int64_t)(sword_t)src1 * (uint64_t)src2, 63, 32));
  INSTPAT("0000001 ????? ????? 011 ????? 0110011", mulhu, R,
          R(rd) = (word_t)BITS((uint64_t)src1 * (uint64_t)src2, 63, 32));
  INSTPAT("0000001 ????? ????? 100 ????? 0110011", div, R,
          R(rd) = (word_t)((sword_t)src1 / (sword_t)src2));
  INSTPAT("0000001 ????? ????? 101 ????? 0110011", divu, R,
          R(rd) = src1 / src2);
  INSTPAT("0000001 ????? ????? 110 ????? 0110011", rem, R,
          R(rd) = (word_t)((sword_t)src1 % (sword_t)src2));
  INSTPAT("0000001 ????? ????? 111 ????? 0110011", remu, R,
          R(rd) = src1 % src2);
  /*instructions for jump*/
  // cpu.pc store the address of jal
  INSTPAT("??????? ????? ????? ??? ????? 11011 11", jal, J, R(rd) = s->snpc,
          s->dnpc = cpu.pc + imm);
  INSTPAT("??????? ????? ????? 000 ????? 11001 11", jalr, I, R(rd) = s->snpc,
          s->dnpc = (imm + src1) & 0xfffffffe);

  /*instructions for branch*/
  INSTPAT("??????? ????? ????? 000 ????? 11000 11", beq, B,
          s->dnpc = src1 == src2 ? imm + s->pc : s->snpc);
  INSTPAT("??????? ????? ????? 001 ????? 11000 11", bne, B,
          s->dnpc = src1 != src2 ? imm + s->pc : s->snpc);
  INSTPAT("??????? ????? ????? 100 ????? 11000 11", blt, B,
          s->dnpc = (sword_t)src1 < (sword_t)src2 ? imm + s->pc : s->snpc);
  INSTPAT("??????? ????? ????? 101 ????? 11000 11", bge, B,
          s->dnpc = (sword_t)src1 >= (sword_t)src2 ? imm + s->pc : s->snpc);
  INSTPAT("??????? ????? ????? 110 ????? 11000 11", bltu, B,
          s->dnpc = src1 < src2 ? imm + s->pc : s->snpc);
  INSTPAT("??????? ????? ????? 111 ????? 11000 11", bgeu, B,
          s->dnpc = src1 >= src2 ? imm + s->pc : s->snpc);

  /*csr instructions*/
  INSTPAT("??????? ????? ????? 001 ????? 11100 11", csrrw, CSR, R(rd) = _csr,
          csr(_csr_addr) = src1);
  INSTPAT("??????? ????? ????? 010 ????? 11100 11", csrrs, CSR, R(rd) = _csr,
          csr(_csr_addr) = csr(_csr_addr) | src1);

  /*instructions for exception*/
  INSTPAT("0011000 00010 00000 000 00000 11100 11", mret, N,
          s->dnpc = csr(MEPC));
  INSTPAT("0000000 00000 00000 000 00000 11100 11", ecall, N,
          s->dnpc = isa_raise_intr(0xb, s->pc)); // pc of current instruction
  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak, N,
          NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv, N, INV(s->pc));
  INSTPAT_END();

  // Log("pc: " FMT_WORD " instruction: " FMT_WORD, cpu.pc, s->isa.inst.val);
  // Log("rd: %2d src1: " FMT_WORD " src2: " FMT_WORD " imm: " FMT_WORD, rd,
  // src1, src2, imm);
  R(0) = 0; // reset $zero to 0

  return 0;
}

int isa_exec_once(Decode *s) {
  s->isa.inst.val = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
