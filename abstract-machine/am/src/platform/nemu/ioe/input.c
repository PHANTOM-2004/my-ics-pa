#include <am.h>
#include <nemu.h>
#include <stdint.h>

#define KEYDOWN_MASK 0x8000
/*
enum { AM_INPUT_KEYBRD = (8) };
typedef struct {
  bool keydown;
  int keycode;
} AM_INPUT_KEYBRD_T;
*/

void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  // keydown = true: it is pressed
  uint32_t const * const key_reg_addr = (uint32_t*)(uintptr_t)KBD_ADDR;// 4 bytes
  uint32_t const keycode = *key_reg_addr;
  kbd->keydown = !!(keycode & KEYDOWN_MASK);
  kbd->keycode = keycode & ~KEYDOWN_MASK; // set KEYDOWN_MASK bit zero
}
