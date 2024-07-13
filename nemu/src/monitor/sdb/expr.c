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
#include "debug.h"
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdbool.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_INT_DEC,
  /* TODO: Add more token types */
  // plus -> +, minus-> -
  // ( -> (. ) -> )
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE}, // spaces
    {"\\+", '+'},      // plus. [CYT] + is special in regex
    {"==", TK_EQ},     // equal
    {"\\(", '('},         {"\\)", ')'}, {"\\-", '-'}, // minus
    {"\\*", '*'},                                     // mult
    {"\\\\", '\\'},                                   // div
    {"\\d+", TK_INT_DEC},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
            rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          // record tokens in array tokens
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
          tokens[nr_token].type =
              rules[i].token_type; // it is simple to just add type
          break;

        case TK_INT_DEC:
          // check the len first;
          if (substr_len > 32) {
            printf("[REGEX ERROR] int too large, result is invalid\n");
            return false;
          }
          int const cpy_len = substr_len < 32 ? substr_len : 32;
          strncpy(tokens[nr_token].str, substr_start, cpy_len);
          break;

        default:
          TODO();
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static int check_paretheses(char const *expr, int const p, int const q) {
  // maybe multiple ()
  int cnt = 0;
  bool paretheses = false;
  for (int i = p; i <= q; i++) {
    if (expr[i] == '(') {
      cnt++;
      paretheses = true;
    } else if (expr[i] == ')')
      cnt--;
    if (cnt < 0)
      return -1; // invalid expression
  }
  return paretheses;
}

static int get_priority(int _op_type) {
  switch (_op_type) {
  case '+':
    return 0;
  case '-':
    return 0;
  case '*':
    return 1;
  case '/':
    return 1;
  default:
    Assert(0, "unknown operator");
    return -1;
  }
}

static word_t expr_eval(char const *expr, int const p, int const q,
                        bool *success) {
  if (p > q) {
    // bad expression
    *success = false;
    return 0;
  } else if (p == q) {
    // trivial expression, it is just a number
    // due to regex, judge whether or not it is number

    word_t const res = (word_t)strtoul(expr, NULL, 10); // dec number
    return res;
  } else {
    // check status
    int res = check_paretheses(expr, p, q);
    if (res < 0) {
      *success = false;
      return 0;
    } // invalid ()
    else if (res)
      return expr_eval(expr, p + 1, q - 1, success);
    else {
      // divide into single expression, according to tokens

      // find the main operator, with the lowest priority
      int main_op_type = -1;
      for (int i = 0; i < nr_token; i++) {
        bool is_main_op = (tokens[i].type == '+') || (tokens[i].type == '-') ||
                          (tokens[i].type == '*') || (tokens[i].type == '/');
        if (!is_main_op)
          continue;
        if (main_op_type < 0)
          main_op_type = tokens[i].type;
        else if (get_priority(tokens[i].type) <= get_priority(main_op_type))
          main_op_type = tokens[i].type; // update main operator
      }

      if (main_op_type < 0) {
        // impossible come here
        *success = false;
        Assert(0, "no main operator\n");
        return 0;
      }

      // divide and conquer
      int left = p, right = q;
      for (int i = p; i <= q; i++) {
        if (expr[i] != main_op_type)
          continue;
        left = i - 1;
        right = i + 1;
      }

      return expr_eval(expr, left, right, success);
    }
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  word_t const res = expr_eval(e, 0, (int)strlen(e) - 1, success);
  return res;

  return 0;
}
