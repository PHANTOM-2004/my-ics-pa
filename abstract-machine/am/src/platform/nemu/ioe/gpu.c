#include <am.h>
#include <nemu.h>
#include <stdint.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)

static uint32_t const *const vga_wh_reg = (uint32_t *)(uintptr_t)VGACTL_ADDR;
static uint32_t vga_width = 0;
static uint32_t vga_height = 0;

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  *cfg = (AM_GPU_CONFIG_T){.present = true,
                           .has_accel = false,
                           .width = vga_width,
                           .height = vga_height,
                           .vmemsz = vga_height * vga_width * sizeof(uint32_t)};
}

/*
// Expands to
enum { AM_GPU_FBDRAW = (11) };
typedef struct {
  int x, y;
  void *pixels;
  int w, h;
  bool sync;
} AM_GPU_FBDRAW_T;

*/
void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *const fb = (uint32_t *)(uintptr_t)FB_ADDR; // locate buffer
  // write to buffer, line first
  for (int i = ctl->y; i < ctl->y + ctl->h; i++)
    for (int j = ctl->x; j < ctl->x + ctl->w; j++)
      fb[i * vga_width + j] =
          ((uint32_t *)ctl->pixels)[(i - ctl->y) * ctl->w + (j - ctl->x)];
  //      ref: io_write(AM_GPU_FBDRAW, x * w, y * h, color_buf, w, h, false);

  if (ctl->sync)
    outl(SYNC_ADDR, 1);
}

void __am_gpu_status(AM_GPU_STATUS_T *status) { status->ready = true; }

void __am_gpu_init() {
  vga_width = (*vga_wh_reg >> 16) & 0xffff;
  vga_height = *vga_wh_reg & 0xffff;

  int const w = vga_width;
  int const h = vga_height;

  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR; // its is 4 byte here
  for (int i = 0; i < w * h; i++)
    fb[i] = 0xffff; // I'd like to clear the screen
  outl(SYNC_ADDR, 1);
}
