#include "am.h"
#include "amdev.h"
#include <common.h>
#include <stdint.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
#define MULTIPROGRAM_YIELD() yield()
#else
#define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) [AM_KEY_##key] = #key,

static const char *keyname[256]
    __attribute__((used)) = {[AM_KEY_NONE] = "NONE", AM_KEYS(NAME)};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  for (size_t i = 0; i < len; i++)
    putch(*((char const *)buf + i));
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T const kbd = io_read(AM_INPUT_KEYBRD);

  if (kbd.keycode == AM_KEY_NONE) // no valid key
    return 0;

  // each time read only on event
  size_t ret = snprintf(buf, len, "%s %s", kbd.keydown ? "kd" : "ku",
                        keyname[kbd.keycode]);
  *((char *)buf + ret) = '\0';
  ret++;
  assert(ret <= len);
  return ret;
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T const gconfig = io_read(AM_GPU_CONFIG);
  if (!gconfig.present)
    return 0;

  size_t ret = snprintf(buf, len, "WIDTH:%d\nHEIGHT:%d\n", gconfig.width,
                        gconfig.height);
  *((char *)buf + ret) = '\0';
  ret++;
  return ret;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  // AM_GPU_FBDRAW_T am_fb[1] = { {.} } io_write(AM_GPU_FBDRAW);
  static uint32_t v_width = 0, v_height = 0;
  if (!v_width || !v_height) {
    AM_GPU_CONFIG_T const gconfig = io_read(AM_GPU_CONFIG);
    v_height = gconfig.height, v_width = gconfig.width;
  }

  uint32_t const w_t = offset % v_width;
  uint32_t const h_t = offset / v_width;
  io_write(AM_GPU_FBDRAW, w_t, h_t, buf, len, 1, true);
  return 0;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
