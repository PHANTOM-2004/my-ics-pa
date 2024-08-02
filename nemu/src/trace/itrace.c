#include "common.h"

#ifdef CONFIG_ITRACE_RINGBUF_ON
#define CHAR_BUF_LEN 128

static char ringbuf[CONFIG_ITRACE_RINGBUF_SIZE][CHAR_BUF_LEN]; // 128 same with
static int ringbuf_pos = 0;
static bool ringbuf_full = false;

void iringbuf_trace(char const *_logbuf) {
  // Log("call iringbuf, config size : %d", CONFIG_ITRACE_RINGBUF_SIZE);
  ringbuf_pos++;
  if (ringbuf_pos == CONFIG_ITRACE_RINGBUF_SIZE)
    ringbuf_pos = 0, ringbuf_full = true;

  memcpy(ringbuf[ringbuf_pos], _logbuf, CHAR_BUF_LEN);
  ringbuf[ringbuf_pos][CHAR_BUF_LEN - 1] = '\0'; // in case _logbuf[127] != 0
}

void print_iringbuf() {
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
