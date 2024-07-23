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

#include <common.h>

void init_monitor(int, char *[]);
void am_init_monitor();
void engine_start();
int is_exit_status_bad();

// these MACROs are for testing and debugging
// ------------------------------------------
#define __EXPR_TEST
#ifdef __EXPR_TEST
#include "monitor/sdb/sdb.h"
#include <readline/readline.h>

static int expr_test() {
  word_t correct_ans = 0;

  char *buffer0 = readline("Enter answer: ");
  assert(buffer0);
  sscanf(buffer0, "%u", &correct_ans);
  free(buffer0);
  Log("correct ans: %u", correct_ans);

  char *buffer = readline("Enter expression: ");
  Log("buffer: %s", buffer);
  Assert(buffer, "buffer cannot be NULL");

  bool success = false;
  word_t res = expr(buffer, &success);

  Log("success: %d", success);
  Log("correct: " FMT_WORD " my: " FMT_WORD, correct_ans, res);
  Log("%s\n", correct_ans == res ? "TRUE" : "FALSE");

  free(buffer);
  return 0;
}

#endif
// ------------------------------------------

int main(int argc, char *argv[]) {

  /* Initialize the monitor. */
#ifdef CONFIG_TARGET_AM
  am_init_monitor();
#else
  init_monitor(argc, argv);
#endif

/*sth test here, testing expression*/
#ifdef __EXPR_TEST
  int loop = 1000;
  while (loop--)
    expr_test();
  return 0;
#undef __EXPR_TEST
#endif

  /* Start engine. */
  engine_start();

  return is_exit_status_bad();
}
