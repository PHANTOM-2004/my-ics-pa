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
#include "difftest-def.h"
#include "isa.h"
#include "utils.h"
#include <cpu/cpu.h>
#include <cpu/decode.h>
#include <cpu/difftest.h>
#include <locale.h>
#include <stdint.h>

/* The assembly code of instructions executed is only output to the screen
 * when the number of instructions executed is less than this value.
 * This is useful when you use the `si' command.
 * You can modify this value as you want.
 */
#define MAX_INST_TO_PRINT 10

CPU_state cpu = {.csr[MSTATUS] = 0x1800};
uint64_t g_nr_guest_inst = 0;
static uint64_t g_timer = 0; // unit: us
static bool g_print_step = false;

void device_update();
int scan_watchpoint();

/* @param dnpc: we pass cpu.pc to dnpc
 * */
#ifdef CONFIG_ITRACE_RINGBUF_ON
#define CHAR_BUF_LEN 128

static char ringbuf[CONFIG_ITRACE_RINGBUF_SIZE][CHAR_BUF_LEN]; // 128 same with
static int ringbuf_pos = 0;
static bool ringbuf_full = false;

static void iringbuf_trace(char const *_logbuf) {
  // Log("call iringbuf, config size : %d", CONFIG_ITRACE_RINGBUF_SIZE);
  ringbuf_pos++;
  if (ringbuf_pos == CONFIG_ITRACE_RINGBUF_SIZE)
    ringbuf_pos = 0, ringbuf_full = true;

  memcpy(ringbuf[ringbuf_pos], _logbuf, CHAR_BUF_LEN);
  ringbuf[ringbuf_pos][CHAR_BUF_LEN - 1] = '\0'; // in case _logbuf[127] != 0
}

static void print_iringbuf() {
  printf("[ITRACE]: recent instruction below[buffer %s full]\n",
         ringbuf_full ? "" : ANSI_FMT("NOT", ANSI_FG_GREEN));
  int const buf_len =
      ringbuf_full ? CONFIG_ITRACE_RINGBUF_SIZE : ringbuf_pos + 1;
  for (int i = 0; i < buf_len; i++) {
    printf("%s ", i == ringbuf_pos ? "  -->\t" : "\t");
    printf("%s\n", ringbuf[i]);
  }
}

#undef CHAR_BUF_LEN
#endif

#ifdef CONFIG_FTRACE
#include <trace/ftrace.h>

static struct {
  Elf32_func const *func;
  vaddr_t pc;
  enum { CALL, RET } status;
} call_stack[CONFIG_FTRACE_STACK_SIZE * 2];
static int stk_ptr = 0;

static void printws(int len) {
  while (len--)
    printf(" ");
}

static void print_ftrace() {
  printf("[FTRACE]: recent instruction below");
  int space = 2;
  for (int i = 0; i < stk_ptr; i++) {
    if (call_stack[i].status == CALL) {
      printf("%08x", call_stack[i].pc);
      printws(space);
      printf("call [%s@%08x]\n", call_stack[i].func->name,
             call_stack[i].func->value);
      space++;
    } else {
      printf("%08x", call_stack[i].pc);
      printws(space);
      printf("ret [%s]\n", call_stack[i].func->name);
      space--;
    }
  }
}

static void ftrace(Decode const *const s, char const *disassemble) {

  static int const ra = 0x1;
  /*rd and auipc for last time */

  uint32_t const i = s->isa.inst.val;
  uint32_t const jalr_offset = BITS(i, 31, 20);
  int const rs1 = BITS(i, 19, 15);
  int const rd = BITS(i, 11, 7);

  bool const is_jal = (uint32_t)BITS(i, 6, 0) == 0b1101111;
  bool const is_jalr = (uint32_t)BITS(i, 6, 0) == 0b1100111 &&
                       (uint32_t)BITS(i, 14, 12) == 0b000;

  bool const is_near_cal = is_jal && rd == ra;
  bool const is_far_call = is_jalr && rd == ra;
  bool const is_call = is_near_cal || is_far_call;

  // imm=0, rs1 = ra, rd = zero; pc = rs1 + imm = ra + 0
  bool const is_ret = is_jalr && !jalr_offset && rs1 == ra && rd == 0x0;

  Assert(stk_ptr >= 0 && stk_ptr < CONFIG_FTRACE_STACK_SIZE,
         "stack only support at most %d", CONFIG_FTRACE_STACK_SIZE);

  Elf32_func const *func = NULL;

  /*if it is call*/
  if (is_call) {
    // add to call stack

    func = get_elffunction(s->dnpc);
    Assert(func, "get elffunction cannot be NULL");
    // Log("%08x %s", s->pc, disassemble);
    Log("call@[%08x@%s]", s->dnpc, func->name);

    call_stack[stk_ptr].func = func;
    call_stack[stk_ptr].pc = s->pc;
    call_stack[stk_ptr].status = CALL;
    stk_ptr++;
    return;
  }

  /* if it is ret*/
  else if (is_ret) {

    func = get_elffunction(s->dnpc);
    Assert(func, "get elffunction cannot be NULL");
    // Log("%08x %s", s->pc, disassemble);
    Log("ret@[%08x] %s", s->dnpc, func->name);

    call_stack[stk_ptr].func = func;
    call_stack[stk_ptr].pc = s->pc;
    call_stack[stk_ptr].status = RET;
    stk_ptr++;
    return;
  }

  // up date last
}

#endif

/*watch point new, watch point free*/
static void trace_and_difftest(Decode *_this, vaddr_t dnpc) {
#ifdef CONFIG_ITRACE_COND
  if (ITRACE_COND) {
    log_write("%s\n", _this->logbuf);
  }
#endif
  if (g_print_step) {
    IFDEF(CONFIG_ITRACE, puts(_this->logbuf));
  }
  IFDEF(CONFIG_DIFFTEST, difftest_step(_this->pc, dnpc));

#ifdef CONFIG_ITRACE_RINGBUF_ON
  iringbuf_trace(_this->logbuf);
  // print_iringbuf();
#endif
  /*scan all the watch point here*/
#ifdef CONFIG_CONFIG_WATCHPOINT
  int const ret = scan_watchpoint();
  if (ret < 0) // hits
    nemu_state.state = NEMU_STOP;
#endif
}

static void exec_once(Decode *s, vaddr_t pc) {
  s->pc = pc;
  s->snpc = pc;
  isa_exec_once(s);
  cpu.pc = s->dnpc;
#ifdef CONFIG_ITRACE
  char *p = s->logbuf;
  p += snprintf(p, sizeof(s->logbuf), FMT_WORD ":", s->pc);
  int ilen = s->snpc - s->pc;
  int i;
  uint8_t *inst = (uint8_t *)&s->isa.inst.val;
  for (i = ilen - 1; i >= 0; i--) {
    p += snprintf(p, 4, " %02x", inst[i]);
  }
  int ilen_max = MUXDEF(CONFIG_ISA_x86, 8, 4);
  int space_len = ilen_max - ilen;
  if (space_len < 0)
    space_len = 0;
  space_len = space_len * 3 + 1;
  memset(p, ' ', space_len);
  p += space_len;

#ifndef CONFIG_ISA_loongarch32r
  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  disassemble(p, s->logbuf + sizeof(s->logbuf) - p,
              MUXDEF(CONFIG_ISA_x86, s->snpc, s->pc),
              (uint8_t *)&s->isa.inst.val, ilen);
#else
  p[0] = '\0'; // the upstream llvm does not support loongarch32r
#endif
#endif
  // Log("[%08x] %s", s->pc, p);
  IFDEF(CONFIG_FTRACE, ftrace(s, p));
}

static void execute(uint64_t n) {
  Decode s;
  for (; n > 0; n--) {
    exec_once(&s, cpu.pc);
    g_nr_guest_inst++;
    trace_and_difftest(&s, cpu.pc);
    if (nemu_state.state != NEMU_RUNNING)
      break;
    IFDEF(CONFIG_DEVICE, device_update());
  }
}

static void statistic() {
  IFNDEF(CONFIG_TARGET_AM, setlocale(LC_NUMERIC, ""));
#define NUMBERIC_FMT MUXDEF(CONFIG_TARGET_AM, "%", "%'") PRIu64
  Log("host time spent = " NUMBERIC_FMT " us", g_timer);
  Log("total guest instructions = " NUMBERIC_FMT, g_nr_guest_inst);
  if (g_timer > 0)
    Log("simulation frequency = " NUMBERIC_FMT " inst/s",
        g_nr_guest_inst * 1000000 / g_timer);
  else
    Log("Finish running in less than 1 us and can not calculate the simulation "
        "frequency");
}

void assert_fail_msg() {
  IFDEF(CONFIG_ITRACE_RINGBUF_ON, print_iringbuf());
  IFDEF(CONFIG_FTRACE, print_ftrace());
  isa_reg_display();
  statistic();
}

/* Simulate how the CPU works. */
void cpu_exec(uint64_t n) {
  g_print_step = (n < MAX_INST_TO_PRINT);
  switch (nemu_state.state) {
  case NEMU_END:
  case NEMU_ABORT:
    printf("Program execution has ended. To restart the program, exit NEMU and "
           "run again.\n");
    return;
  default:
    nemu_state.state = NEMU_RUNNING;
  }

  uint64_t timer_start = get_time();

  execute(n);

  uint64_t timer_end = get_time();
  g_timer += timer_end - timer_start;

  switch (nemu_state.state) {
  case NEMU_RUNNING:
    nemu_state.state = NEMU_STOP;
    break;

  case NEMU_END:
  case NEMU_ABORT:
    Log("nemu: %s at pc = " FMT_WORD,
        (nemu_state.state == NEMU_ABORT
             ? ANSI_FMT("ABORT", ANSI_FG_RED)
             : (nemu_state.halt_ret == 0
                    ? ANSI_FMT("HIT GOOD TRAP", ANSI_FG_GREEN)
                    : ANSI_FMT("HIT BAD TRAP", ANSI_FG_RED))),
        nemu_state.halt_pc);
    // fall through
  case NEMU_QUIT:
    statistic();
    IFDEF(CONFIG_ITRACE_RINGBUF_ON, print_iringbuf());
    IFDEF(CONFIG_FTRACE, print_ftrace());
  }
}
