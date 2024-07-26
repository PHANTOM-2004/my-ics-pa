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

#include "memory/paddr.h"

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
  TK_INT_HEX, // 0x
  TK_DEREF,   // pointer type
  TK_UNEQ,
  TK_LAND,
  TK_REG
};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {"0x[0-9]+", TK_INT_HEX},
    {"([0-9]+)|([0-9]+[uU])", TK_INT_DEC}, // \d+
    {" +", TK_NOTYPE},                     // spaces
    {"\\+", '+'},                          // plus. [CYT] + is special in regex
    {"==", TK_EQ},                         // equal
    {"\\(", '('},                          // \(      {"\\)", ')'}, //
    {"\\)", ')'},                          // \)
    {"\\-", '-'},                          // \-
    {"\\*", '*'},                          // \*
    {"/", '/'},                            // /

    /*These are extended tokesn*/
    {"\\*", TK_DEREF}, // actually it useless here,
    {"!=", TK_UNEQ},
    {"&&", TK_LAND},
    //{"\\$(0|[12][0-9]|3[01])", TK_REG},
    {"\\$0|ra|sp|gp|tp|t[0-6]|s[01]|a[0-7]|s[2-9]|s1[01]", TK_REG},
};

static bool binary_operator(int const tk_type) {
  return tk_type == '+' || tk_type == '-' || tk_type == '/' || tk_type == '*' ||
         tk_type == TK_UNEQ || tk_type == TK_EQ || tk_type == TK_LAND;
}

static bool is_operator(int const tk_type) {
  return binary_operator(tk_type) || tk_type == TK_DEREF || tk_type == TK_REG;
};

static char const *op_type_to_str(int const op_type) {
  switch (op_type) {
  case '+':
    return "+";
  case '-':
    return "-";
  case '*':
    return "*";
  case '/':
    return "/";
  case TK_LAND:
    return "&&";
  case TK_EQ:
    return "==";
  case TK_UNEQ:
    return "!=";

  default:
    return NULL;
  }
}

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

  Log("make token for: %s", e);
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
        case TK_EQ:
        case TK_UNEQ:
        case TK_DEREF:
        case TK_NOTYPE:
        case TK_LAND:
          tokens[nr_token].type =
              rules[i].token_type; // it is simple to just add type
          break;

        case TK_REG:
          if (substr_len > 2 + 1) // invalid reg
          {
            Log("[REGEX ERROR] reg invalid");
            return false;
          }
          tokens[nr_token].type = rules[i].token_type;
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          break;

        case TK_INT_DEC:
        case TK_INT_HEX:
          // check the len first;

          if (substr_len > 10) {
            // at most 10 for 2147483648*2-1, and 0xffffffff
            Log("[REGEX ERROR] int too large, result is invalid");
            return false;
          }

          tokens[nr_token].type = rules[i].token_type;
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
    Log("total tokens : %d", nr_token);
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
  case TK_EQ:
  case TK_UNEQ:
    return -1; // eg: a == b || b == c

  case TK_LAND:
    return -2;

  case TK_DEREF: // deref first
  case TK_REG:   // $ first
    return 2;

  case '+':
  case '-':
    return 0;

  case '*':
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
    bool const is_main_op = cnt == 0 && is_operator(tokens[i].type);

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

static word_t _arith_dual_calc(int const tk, word_t val1, word_t val2,
                               bool *success) {
  Assert(binary_operator(tk), "Not biniary operator");
  word_t res = 0;
  switch (tk) {
  case '+':
    res = val1 + val2;
    break;
  case '-':
    res = val1 - val2;
    break;
  case '*':
    res = val1 * val2;
    break;
  case '/':
    if (val2 == 0) {
      *success = false;
      Log("[DIVISION ERROR]Divisor cannot be zero");
      return 0;
    }
    res = val1 / val2;
    break;

  case TK_LAND:
    res = val1 && val2;
    break;
  case TK_EQ:
    res = val1 == val2;
    break;
  case TK_UNEQ:
    res = val1 != val2;
    break;

  default:
    Log("Unknown operator: %d", tk);
    TODO();
  }

  Log("tmp expr:%u %s %u = %u", val1, op_type_to_str(tk), val2, res);
  return res;
}

static word_t get_tk_number(int const p, bool *success) {
  int const tk = tokens[p].type;
  char const *const str = tokens[p].str;
  Log("tk_number: %d", tk);
  word_t res = 0;

  switch (tk) {
  case TK_INT_DEC:
    res = (word_t)strtoul(str, NULL, 10);
    break;
  case TK_INT_HEX:
    Assert(str[0] == '0' && str[1] == 'x', "hex number must be 0x**..*");
    res = (word_t)strtoul(str + 2, NULL, 16);
    break;
  case TK_REG:
    res = isa_reg_str2val(str, success);
    break;
  default:
    Assert(0, "token here must be number/register");
  }

  Log("tk_number read: %s", str);
  Log("tk_number get: " FMT_WORD, res);
  return res;
}

static word_t expr_eval(char const *const expr, int _p, int _q,
                        bool *const success) {
  // we need to trim ()
  if(!*success || _p > _q ){
    *success = false;
    return 0;
  }

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
    // trivial expression, it is just a number hex or dec
    // due to regex, judge whether or not it is number
    return get_tk_number(p, success);
  }

  else {

    int res = check_paretheses(p, q);
    if (res < 0) {
      Log("[SYNTAX ERROR] not valid ()");
      *success = false;
      return 0;
    } // invalid ()
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
    if(!binary_operator(main_op_type)){
      Log("[SYNTAX ERROR] invalid expression");
      *success = false;
      return 0;
    }
    Log("main op: %s", op_type_to_str(main_op_type));

    // special for deref *, if there is deref,
    // there must deref only, since deref has highest priority
    // these are for single operator
    if (main_op_type == TK_DEREF) {
      word_t const addr = expr_eval(expr, main_op_pos + 1, q, success);
      if (!likely(in_pmem(addr))) {
        Log("[ERROR] address out of bound");
        *success = false;
        return 0;
      } // out of bound
      word_t const read_mem = paddr_read(addr, 4);
      Log("[MEMORY] read from " FMT_WORD ": " FMT_WORD, addr, read_mem);
      return read_mem;
    }

    // divide and conquer
    word_t const val1 = expr_eval(expr, p, main_op_pos - 1, success);
    word_t const val2 = expr_eval(expr, main_op_pos + 1, q, success);

    word_t const arith_res =
        _arith_dual_calc(main_op_type, val1, val2, success);

    return arith_res;
  }
}

static bool _deref_special_tk(int const pre_tk) {
  // the prev is operator not number, or prev is just a (
  return is_operator(pre_tk) || pre_tk == '(';
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  *success = true;
  /* TODO: Insert codes to evaluate the expression. */

  for (int i = 0; i < nr_token; i++) {
    if (tokens[i].type == '*' &&
        (i == 0 || _deref_special_tk(tokens[i - 1].type)))
      tokens[i].type = TK_DEREF; // this is to judge for pointer
  }

  word_t const res = expr_eval(e, 0, nr_token - 1, success);
  return res;
}
