#ifndef __FTRACE_H__
#define __FTRACE_H__

#include "common.h"

typedef struct {
  uint32_t value;
  uint32_t size;
  char name[1 + CONFIG_FTRACE_FUNC_NAME_LIMIT];
} Elf32_func;


int init_elf();
Elf32_func const * get_elffunction(vaddr_t const target);

#endif
