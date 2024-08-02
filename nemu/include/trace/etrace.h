#include "common.h"

#ifdef CONFIG_ETRACE
#include <cpu/decode.h>
void eringbuf_trace(Decode *s);
void print_eringbuf();
#endif
