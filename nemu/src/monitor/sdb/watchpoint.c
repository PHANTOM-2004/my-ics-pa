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
#include "sdb.h"

#define NR_WP 64
#define INIT_BYTE 0xffffffff

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */
  char *w_expr;  // store the expression
  word_t val;    // store the value last time
  int hits_time; // hits how many times
} WP;

static WP wp_pool[NR_WP] = {};
static WP special_ptr[3] = {};
static WP *head = &special_ptr[0], *free_ = &special_ptr[1];
static WP *_front = &special_ptr[2];
static int allocated_nr = 0;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i++) {
    wp_pool[i].val = INIT_BYTE;
    wp_pool[i].w_expr = NULL;
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].hits_time = 0;
  }

  head->next = NULL;
  free_->next = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP *new_wp() {

  Assert(allocated_nr < NR_WP, "nothing to alloc");
  // insert into head,
  // delete from free_
  WP *tmp = NULL;
  tmp = head->next; // record the origin one

  Assert(free_->next, "out of watch point memory");
  Assert(free_->next->w_expr == NULL, "all new node should be NULL");

  head->next = free_->next;
  free_->next = free_->next->next;

  head->next->next = tmp;
  head->next->w_expr = NULL;
  head->next->hits_time = 0;
  head->next->val = INIT_BYTE;
  head->next->NO = INIT_BYTE;

  allocated_nr++;
  return head->next;
}

void free_wq(WP *wp) {
  Assert(wp, "cannot free NULL");
  Assert(allocated_nr, "nothing to free");
  WP *tmp = NULL;

  /*add to free*/
  tmp = free_->next;
  free_->next = wp;
  wp->next = tmp;

  /*delete from head*/
  bool find = false;
  // we need p->next, so cannot start from p->head
  for (WP *p = head; p != NULL; p = p->next) {
    if (p->next == wp) {
      p->next = p->next->next;
      find = true;
      break;
    }
  }
  Assert(find, "not found what to be freed");

  allocated_nr--;
}

/*
 * @return: return 0 when ok
 * return < 0 when error
 */
int wp_insert(char *_expr) {
  static int w_id = 0;   // each id is unique
  if (allocated_nr == 0) // it is clean
    w_id = 0;

  WP *_new_node = new_wp();
  WP *tmp = _front->next;
  _front->next = _new_node;
  _new_node->next = tmp;
  _new_node->NO = w_id;

  size_t const len = strlen(_expr);
  bool success = true;
  _new_node->w_expr = (char *)malloc(sizeof(char) * (len + 1));
  Log("wp_insert: malloc %d", (int)sizeof(char) * (int)(len + 1));
  strncpy(_new_node->w_expr, _expr, len + 1);
  _new_node->w_expr[len] = '\0';
  _new_node->val = expr(_new_node->w_expr, &success);
  if(!success){
    Log("[SYNTAX ERROR] insert invalid wathpoint");
    return 0;
  }

  Log("insert wp: %s", _expr);
  w_id++;
  return 0;
}

/*
 * @return: return 0 when ok
 * return < 0 when error
 */
int wp_delete(int const id) {
  for (WP *p = _front; p != NULL; p = p->next) {
    if (p->next && p->next->NO == id) {
      WP *to_be_free = p->next;
      p->next = to_be_free->next;

      free(to_be_free->w_expr);
      free_wq(to_be_free);

      Log("delete wp: %d", id);
      return 0;
    }
  }
  Log("[WP ERROR] cannot find wp id: %d", id);
  return -1;
}

int print_watchpoint() {
  for (WP const *p = _front->next; p != NULL; p = p->next) {
    printf("WP[%02d]: hits:%3d\n", p->NO, p->hits_time);
    Assert(p->w_expr, "expression == NULL");
    printf("expression: [ %s ]\n", p->w_expr);
  }
  return 0;
}

/*
 * @return: return 0 when nothing change
 * return < 0 when sth change
 */
int scan_watchpoint() {
  int res = 0;
  for (WP *p = _front->next; p != NULL; p = p->next) {
    bool success = false;
    word_t const cur_val = expr(p->w_expr, &success);
    if (cur_val == p->val)
      continue;
    res = -1;
    p->hits_time++;
    printf("[watchpoint %d] hits\n", p->NO);
    printf("expression: [ %s ]\n", p->w_expr);
    printf("old value: " FMT_WORD "\nnew value: " FMT_WORD "\n", p->val,
           cur_val);
    p->val = cur_val;
  }
  return res;
}
