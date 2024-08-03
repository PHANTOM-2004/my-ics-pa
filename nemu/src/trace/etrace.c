#include "common.h"
#include "isa-def.h"
#include <stdint.h>

#ifdef CONFIG_ETRACE
#include "../local-include/reg.h"
#include "isa.h"
#include <cpu/decode.h>
#define CHAR_BUF_LEN 128
typedef struct {
  char rb[CHAR_BUF_LEN];
  word_t mtvec;
  word_t mcause;
  word_t mstatus;
  word_t mepc;
} Eringbuffer;

static Eringbuffer eringbuf[CONFIG_ETRACE_RINGBUF_SIZE]; // 128 same with
static int eringbuf_pos = 0;
static bool eringbuf_full = false;

enum {
  ECALL = 0b1110011,
  EBREAK = 0x100073,
};

void eringbuf_trace(Decode *s) {
  // Log("call eringbuf, config size : %d, instruction: " FMT_WORD,
  // CONFIG_ETRACE_RINGBUF_SIZE, s->isa.inst.val);
  if (!(EBREAK == s->isa.inst.val || ECALL == s->isa.inst.val))
    return;
  eringbuf_pos++;
  if (eringbuf_pos == CONFIG_ETRACE_RINGBUF_SIZE)
    eringbuf_pos = 0, eringbuf_full = true;
  memcpy(eringbuf[eringbuf_pos].rb, s->logbuf, CHAR_BUF_LEN);
  eringbuf[eringbuf_pos].rb[CHAR_BUF_LEN - 1] =
      '\0'; // in case _logbuf[127] != 0
  eringbuf[eringbuf_pos].mcause = csr(MCAUSE);
  eringbuf[eringbuf_pos].mtvec = csr(MTVEC);
  eringbuf[eringbuf_pos].mstatus = csr(MSTATUS);
  eringbuf[eringbuf_pos].mepc = csr(MEPC);
}

void print_eringbuf() {
  Log("print eringbuf");
  printf("[ETRACE]: recent instruction below[buffer %s full]\n",
         eringbuf_full ? "" : ANSI_FMT("NOT", ANSI_FG_GREEN));
  int const buf_len =
      eringbuf_full ? CONFIG_ETRACE_RINGBUF_SIZE : eringbuf_pos + 1;
  for (int i = 0; i < buf_len; i++) {
    if (!i && !eringbuf_full)
      continue;
    printf("%s ", i == eringbuf_pos ? "  -->\t" : "\t");
    printf("%s\n", eringbuf[i].rb);
    printf("%s ", i == eringbuf_pos ? "  -->\t" : "\t");
    printf("[%s]: " FMT_WORD "\t", "mcause", eringbuf[i].mcause);
    printf("[%s]: " FMT_WORD "\t", "mtvec", eringbuf[i].mtvec);
    printf("[%s]: " FMT_WORD "\t", "mstatus", eringbuf[i].mstatus);
    printf("[%s]: " FMT_WORD "\n", "mepc", eringbuf[i].mepc);
  }
}

#undef CHAR_BUF_LEN
#endif
