#include "klib-macros.h"
#include <am.h>
#include <nemu.h>
#include <stdint.h>


void __am_timer_init() {
}

/*
enum { AM_TIMER_UPTIME = (6) };
typedef struct {
  uint64_t us;
} AM_TIMER_UPTIME_T;
*/
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  // guarantee visit high first, that is when time is updated
  uint64_t const _read_time_hi = *(uint32_t *)(uintptr_t)(RTC_ADDR + 4);
  uint64_t const _read_time_lo = *(uint32_t *)(uintptr_t)(RTC_ADDR);

  uint64_t const _read_time = (_read_time_hi << 32) | _read_time_lo;
  uptime->us = _read_time ;//- boot_time.us;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour = 0;
  rtc->day = 0;
  rtc->month = 0;
  rtc->year = 1900;
}
