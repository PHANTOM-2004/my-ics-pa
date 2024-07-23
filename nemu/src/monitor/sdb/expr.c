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

    {"([0-9]+)|([0-9]+[uU])", TK_INT_DEC}, // \d+
    {" +", TK_NOTYPE},                     // spaces
    {"\\+", '+'},                          // plus. [CYT] + is special in regex
    {"==", TK_EQ},                         // equal
    {"\\(", '('},                          // \(      {"\\)", ')'}, //
    {"\\)", ')'},                          // \)
    {"\\-", '-'},                          // \-
    {"\\*", '*'},                          // \*
    {"/", '/'},                            // /
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

static Token tokens[128] __attribute__((used)) = {};
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
        case TK_NOTYPE:
          tokens[nr_token].type =
              rules[i].token_type; // it is simple to just add type
          break;

        case TK_INT_DEC:
          // check the len first;
          if (substr_len > 10) { // at most 10 for 2147483647
            printf("[REGEX ERROR] int too large, result is invalid\n");
            return false;
          }

          tokens[nr_token].type = TK_INT_DEC;
          int const cpy_len = substr_len < 10 ? substr_len : 10;
          strncpy(tokens[nr_token].str, substr_start, cpy_len);
          break;

        default:
          Log("token type error: %d", rules[i].token_type);
          TODO();
        }
        nr_token++; // a match
        break;      // jump out of loop
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}

static int check_paretheses(int const p, int const q) {
  // maybe multiple ()
  int cnt = 0;
  for (int i = p; i <= q; i++) {
    if (tokens[i].type == '(') {
      cnt++;
    } else if (tokens[i].type == ')')
      cnt--;
    if (cnt < 0)
      return -1; // invalid expression
  }

  if (tokens[p].type != '(')
    return false; // not surround by ()

  cnt = 0;
  for (int i = p + 1; i <= q - 1; i++) {
    if (tokens[i].type == '(')
      cnt++;
    else if (tokens[i].type == ')')
      cnt--;
    if (cnt < 0)
      return false; // not surround by valid ()
  }
  return true;
}

static bool is_op_valid(int const _op_type) { return _op_type >= 0; }
static int get_op_priority(int const _op_type) {
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

static int find_main_op(int const p, int const q) {
  int main_op_type = -1;
  int cnt = 0;
  int pos = -1;
  // main op should not be surround by ()
  for (int i = p; i <= q; i++) {
    cnt += tokens[i].type == '(';
    cnt -= tokens[i].type == ')';
    bool const is_main_op =
        cnt == 0 && ((tokens[i].type == '+') || (tokens[i].type == '-') ||
                     (tokens[i].type == '*') || (tokens[i].type == '/'));
    if (!is_main_op)
      continue;
    // find main operator
    if (!is_op_valid(main_op_type)) {
      main_op_type = tokens[i].type;
      pos = i;
    } else if (get_op_priority(tokens[i].type) <=
               get_op_priority(main_op_type)) {
      main_op_type = tokens[i].type; // update main operator
      pos = i;
    }
  }

  return pos;
}

static word_t expr_eval(char const *const expr, int _p, int _q,
                        bool *const success) {
  Log("before: p%d, q:%d", _p, _q);
  while (tokens[_p].type == TK_NOTYPE)
    ++_p;
  while (tokens[_q].type == TK_NOTYPE)
    --_q;
  Log("trimmed: p:%d, q:%d", _p, _q);

  int const p = _p, q = _q;

  if (p > q) {
    // bad expression
    *success = false;
    return 0;
  }

  else if (p == q) {
    // trivial expression, it is just a number
    // due to regex, judge whether or not it is number
    return (word_t)strtoul(tokens[p].str, NULL, 10); // dec number
  }

  else {

    int res = check_paretheses(p, q);
    if (res < 0) {
      Log("[SYNTAX ERROR] not valid ()");
      *success = false;
      return 0;
    } // invalid ()

    // we need to trim ()
    else if (res)
      return expr_eval(expr, p + 1, q - 1, success);

    // divide into single expression, according to tokens
    // find the main operator, with the lowest priority
    int const main_op_pos = find_main_op(p, q);
    if (main_op_pos < 0) {
      // impossible come here
      *success = false;
      Log("[SYNTAX ERROR] no main operator");
      // Assert(0, "no main operator\n");
      return 0;
    }
    int const main_op_type = tokens[main_op_pos].type;

    Log("main op: %c", main_op_type);

    // divide and conquer
    word_t const val1 = expr_eval(expr, p, main_op_pos - 1, success);
    word_t const val2 = expr_eval(expr, main_op_pos + 1, q, success);

    word_t algo_res = 0;
    switch (main_op_type) {
    case '+':
      algo_res = val1 + val2;
      break;
    case '-':
      algo_res = val1 - val2;
      break;
    case '*':
      algo_res = val1 * val2;
      break;
    case '/':
      if (val2 == 0) {
        Log("[DIVISION ERROR]Divisor cannot be zero");
        *success = false;
        return 0;
      }
      algo_res = val1 / val2;
      break;
    default:
      Log("[UNKNOWN OPERATOR] %c", main_op_type);
      return 0;
    }
    Log("tmp expr:%u %c %u = %u", val1, (char)main_op_type, val2, algo_res);
    return algo_res;
  }
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  *success = true;
  /* TODO: Insert codes to evaluate the expression. */
  word_t const res = expr_eval(e, 0, nr_token - 1, success);
  return res;
}
