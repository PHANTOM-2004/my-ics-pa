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

#include "sdb.h"
#include "common.h"
#include "memory/paddr.h"
#include "utils.h"
#include <cpu/cpu.h>
#include <ctype.h>
#include <isa.h>
#include <readline/history.h>
#include <readline/readline.h>

static int is_batch_mode = false;
#define CMD_ERROR_OUTPUT_FMT "[cmd:%s] [DO NOTHING] DUE TO %s"

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static bool _str_isdigit(char const *_num) {
  for (; *_num; ++_num) {
    if (!isdigit(*_num))
      return false;
  }
  return true;
}
static int cmd_info(char *args);
static int cmd_q(char *args);
static int cmd_c(char *args);
static int cmd_si(char *args);
static int cmd_help(char *args);
static int cmd_x(char *args);
static int cmd_info(char *args);
static int cmd_p(char *args);
static int cmd_w(char *args);
static int cmd_d(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},

    /* TODO: Add more commands */
    {"si", "Step Instruction [N]", cmd_si},
    {"info", "Show program status", cmd_info},
    {"x", "Scan memory", cmd_x},
    {"p", "Caculate expression", cmd_p},
    {"d", "Delete watchpoint", cmd_d},
    {"w", "Add watchpoint", cmd_w},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT; // set the flag, inorder to quit normally
  return -1;
}

static int cmd_p(char *args) {
  if (args == NULL || !strlen(args)) {
    Log(CMD_ERROR_OUTPUT_FMT, "p", "no expression");
    return 0;
  }
  Log("cmd_p: %s", args);

  bool success = true;
  word_t const res = expr(args, &success);
  if (!success) {
    Log(CMD_ERROR_OUTPUT_FMT" : [%s]", "p", "invalid expression", args);
    return 0;
  }

  printf("[%s] = " FMT_WORD "\n", args, res);

  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL || !strlen(args)) {
    Log(CMD_ERROR_OUTPUT_FMT, "w", "no expression");
    return 0;
  }
  Log("cmd_w: %s", args);

  int const res = wp_insert(args);
  return res;
}

static int cmd_d(char *args) {
  char *_num_str = strtok(NULL, " ");
  if (_num_str == NULL) {
    Log(CMD_ERROR_OUTPUT_FMT, "d", "lack of number");
    return 0;
  }
  if (!_str_isdigit(_num_str)) {
    Log(CMD_ERROR_OUTPUT_FMT, "d", "invalid number");
    return 0;
  }
  int const id = atoi(_num_str);
  int const res = wp_delete(id);
  return res;
}

static int cmd_x(char *args) {
  // parse the args, needed N and expr.
  // need to be done after calculation of EXPR
  // printf("%s\n", args);
  char * const _str_number = strtok(NULL, " ");
  if (_str_number == NULL) {
    Log(CMD_ERROR_OUTPUT_FMT, "x", "lack of number");
    return 0;
  }
  if (!_str_isdigit(_str_number)) {
    Log(CMD_ERROR_OUTPUT_FMT, "x", "invalid number");
    return 0;
  }
  int const _number = atoi(_str_number);

  char *const _expr = _str_number + strlen(_str_number) + 1;
  if (_expr == NULL || !strlen(_expr)) {
    Log(CMD_ERROR_OUTPUT_FMT, "x", "lack of expression");
    return 0;
  }
  // calc expression
  bool success = true;
  word_t const addr = expr(_expr, &success);
  if (success == false) {
    Log(CMD_ERROR_OUTPUT_FMT" : [%s]", "x", "invalid expression", _expr);
    return 0;
  }

  //
  Log("read memory: [" FMT_WORD "-" FMT_WORD "]", addr, addr + _number - 1);
  if (!likely(in_pmem(addr)) || !likely(in_pmem(addr + _number - 1))) {
    Log(CMD_ERROR_OUTPUT_FMT, "x", "out of range");
    return 0;
  } // prediction

  for (word_t pos = addr; pos < addr + _number; pos+=4) {
    // each time a word
    word_t const value = paddr_read(pos, 4);
    printf(FMT_WORD ":\t\t" FMT_WORD "\n", pos, value);
  }

  return 0;
}

static int cmd_info(char *args) {
  char *target = strtok(NULL, " ");
  if (target == NULL || strlen(target) > 1) {
    Log(CMD_ERROR_OUTPUT_FMT, "info",
        target ? "invalid argument" : "insufficient arguments");
    return 0;
  }
  switch (*target) {
  case 'r':
    isa_reg_display();
    break;
  case 'w':
    // TODO: print watch points
    print_watchpoint();
    break;
  default:
    Log(CMD_ERROR_OUTPUT_FMT, "info", "unknown argument");
    return 0;
  }
  return 0;
}

static int cmd_si(char *args) {
  char *number = strtok(NULL, " "); // delimiter = space
  for (char *ptr = number; ptr; ptr++) {
    if (!isdigit(ptr)) {
      Log(CMD_ERROR_OUTPUT_FMT, "si", "non-digit input");
      return 0;
    }
  }

  int const times = number ? atoi(number) : 1;
  cpu_exec(times);
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
