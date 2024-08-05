#pragma once

#include "common.h"

#define ANSI_FMT_GREEN(fmt) "\33[1;35m" fmt "\33[0m"

#ifdef CONFIG_STRACE
int strace(uintptr_t const ev_type, uintptr_t const a0, uintptr_t const a1,
           uintptr_t const a2);
void print_strace();
#endif
