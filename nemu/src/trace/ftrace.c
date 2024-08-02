#include <common.h>

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

void print_ftrace() {
  printf("[FTRACE]: recent instruction below\n");
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

void ftrace(Decode const *const s) {

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
    Log("call@[%08x@%s]", s->dnpc, func->name);
    Assert(func, "get elffunction cannot be NULL");
    // Log("%08x %s", s->pc, disassemble);

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
