#include <NDL.h>
#include <sdl-timer.h>
#include <stdio.h>

SDL_TimerID SDL_AddTimer(uint32_t interval, SDL_NewTimerCallback callback, void *param) {
  return NULL;
}

int SDL_RemoveTimer(SDL_TimerID id) {
  return 1;
}

uint32_t __SDL_INIT_TIME = 0;

uint32_t SDL_GetTicks() {
  /*(Uint32) Returns an unsigned 32-bit value representing the number of 
   * milliseconds since the SDL library initialized.*/
  return NDL_GetTicks() - __SDL_INIT_TIME;
}

void SDL_Delay(uint32_t ms) {
}
