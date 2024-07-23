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
#include <assert.h>
#include <readline/chardefs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format = "#include <stdio.h>\n"
                           "int main() { "
                           "  unsigned result = %s; "
                           "  printf(\"%%u\", result); "
                           "  return 0; "
                           "}";
static int const MAX_BUF_LENGTH = 100;
static int const EXPR_TREE_NODE_SIZE = 10 + 1; // at most 10 number, and a u

static int choose(int const n) { return (uint32_t)rand() % n; }

enum {
  OP_PLUS = '+',
  OP_MINUS = '-',
  OP_MULT = '*',
  OP_DIV = '/',
  OP_EQ = 0,
  OP_UNEQ,
  OP_LAND,
};

struct Operator {
  int type;
  char const *const str;
} operators[] = {
    {OP_PLUS, "+"}, {OP_MINUS, "-"}, {OP_MULT, "*"},  {OP_DIV, "/"},
    {OP_EQ, "=="},  {OP_UNEQ, "!="}, {OP_LAND, "&&"},
};
static int const OP_MAX_LEN = 2;
static int const nr_operators = sizeof(operators) / sizeof(operators[0]);

static uint32_t gen_num(int *buf_pos,
                        int const spare_buf_len) { // rand() only is ok;
  assert(spare_buf_len >= 0);
  uint32_t res = 0;
  uint32_t n = (uint32_t)(unsigned short)rand();
  res = n;
  char stk_buf[12]; // last pos is 11
  int pos = 11, len = 0;

  len++, stk_buf[pos--] = 'u'; // add a u
  do {
    int const num = n % 10;
    n /= 10;
    stk_buf[pos--] = num + '0';
    len++;
  } while (n);

  /*we do not want to read a u*/

  strncpy(buf + *buf_pos, stk_buf + pos + 1, len);
  *buf_pos += len; // *buf_len += len;
  buf[*buf_pos] = '\0';

  return res;
}

static void gen(int *buf_pos, int const spare_buf_len, char const ch) {
  if (spare_buf_len < 0) {
    printf("%d\n", spare_buf_len);
    assert(spare_buf_len >= 0);
  }

  buf[*buf_pos] = ch;
  *buf_pos += 1;
  buf[*buf_pos] = '\0';
}
static int gen_op(int *buf_pos, int const spare_buf_len) {
  // op only fout;
  int const pos = choose(nr_operators);
  int const _op_len = strlen(operators[pos].str);

  for (int i = 0; i < _op_len; i++)
    gen(buf_pos, spare_buf_len, operators[pos].str[i]);
  return pos;
}

static uint32_t op_calc(int const op, uint32_t const val1,
                        uint32_t const val2) {
  uint32_t res = 0;
  switch (op) {
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
    assert(val2);
    res = val1 / val2;
    break;
    // TODO: more operator
  case OP_UNEQ:
    res = val1 != val2;
    break;
  case OP_EQ:
    res = val1 == val2;
    break;
  case OP_LAND:
    res = val1 && val2;
    break;

  default:
    fprintf(stderr, "unknown operator: %d\n", op);
    assert(0);
  }
  // fprintf(stderr, "%u %d(%c) %u = %u\n", val1, op, (char)op, val2, res);
  // fflush(stdout);
  return res;
}
static uint32_t _gen_rand_expr(int *buf_pos, int *spare_buf_len) {
  // to guarantee that it does not overflow

  int t = choose(3);

  // printf("len: %d, t: %d, pos: %d\n", *spare_buf_len, t, *buf_pos);
  // printf("%s\n", buf);

  uint32_t res = 0;
  uint32_t val1 = 0, val2 = 0;
  int op_type = 0;
  int buf_pos_bak = 0;
  int spare_buf_len_bak = 0;

  // insert space
  if (*spare_buf_len > 0 && choose(2))
    gen(buf_pos, --*spare_buf_len, ' ');

  if ((t == 1 && *spare_buf_len < 2) || // 2 for ()
      (t == 2 &&
       (*spare_buf_len < EXPR_TREE_NODE_SIZE + OP_MAX_LEN))) // 2 for op
    t = 0;

  // printf("t: %d expr: %s\n", t, buf);
  // printf("pos: %d len: %d\n", *buf_pos, *spare_buf_len);
  switch (t) {
  case 0:
    assert(*spare_buf_len >= 0);
    res = gen_num(buf_pos, *spare_buf_len);
    break;

  case 1:
    *spare_buf_len -= 2;
    gen(buf_pos, *spare_buf_len, '(');
    res = _gen_rand_expr(buf_pos, spare_buf_len);
    gen(buf_pos, *spare_buf_len, ')');
    break;

  case 2:
    *spare_buf_len -= EXPR_TREE_NODE_SIZE; // split into 2 nodes
    *spare_buf_len -= OP_MAX_LEN;          // op

    /*generate val1*/
    val1 = _gen_rand_expr(buf_pos, spare_buf_len);
    /*generate operator */
    op_type = operators[gen_op(buf_pos, *spare_buf_len)].type;
    /*generate val2*/
    buf_pos_bak = *buf_pos;             // backup pos
    spare_buf_len_bak = *spare_buf_len; // backup spare len

    do {
      *buf_pos = buf_pos_bak;             // redo gen expr
      *spare_buf_len = spare_buf_len_bak; // redo gen expr
      val2 = _gen_rand_expr(buf_pos, spare_buf_len);
      if (val2)
        res = op_calc(op_type, val1, val2);
    } while (op_type == '/' && (!val2));

    break;
    // end of case 2
  }

  // insert space
  if (*spare_buf_len > 0 && choose(2)) {
    gen(buf_pos, --*spare_buf_len, ' ');
  }

  return res;
}

static void gen_rand_expr() {
  int buf_pos = 0;
  int spare_buf_len = MAX_BUF_LENGTH;
  spare_buf_len -= EXPR_TREE_NODE_SIZE;

  buf[0] = '\0';
  // TODO: how to uint32_t int?
  _gen_rand_expr(&buf_pos, &spare_buf_len);
}

int main(int argc, char *argv[]) {
  int seed = time(0); // time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc -Werror /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) // we treat warning as error, if there is divide 0 error;
    {
      i--;
      continue;
    }

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);
    printf("%u\n%s\n", result, buf);
  }
  return 0;
}
