#include <NDL.h>
#include <SDL.h>
#include <assert.h>
#include <string.h>

#define keyname(k) #k,

static const char *keyname[] = {"NONE", _KEYS(keyname)};
#define KEYCODE_NUM sizeof(keyname) / sizeof(keyname[0])

int SDL_PushEvent(SDL_Event *ev) { return 0; }

/*typedef struct {
  uint8_t sym;
} SDL_keysym;

typedef struct {
  uint8_t type;
  SDL_keysym keysym;
} SDL_KeyboardEvent;
*/

int SDL_PollEvent(SDL_Event *ev) {
  /*(int) Returns 1 if there is a pending event or 0 if there are none
   * available.*/
  char event_buf[64] = "\0";
  int ret = NDL_PollEvent(event_buf, sizeof(event_buf));
  if (!ret) {
    ev->type = SDL_NONE;
    ev->key.keysym.sym = SDLK_NONE; // no key event
    return 0;
  }
  char keydown_buf[4] = "\0";
  char keycode_buf[32] = "\0";
  ret = sscanf(event_buf, "%s %s", keydown_buf, keycode_buf);
  assert(ret);

  if (strcmp(keydown_buf, "kd") == 0) {
    ev->type = SDL_KEYDOWN;
    ev->key.type = SDL_KEYDOWN;
  } else if (strcmp(keydown_buf, "ku") == 0) {
    ev->type = SDL_KEYUP;
    ev->key.type = SDL_KEYUP;
  } else
    assert(0);

  for (int i = 0; i < KEYCODE_NUM; i++) {
    if (strcmp(keyname[i], keycode_buf))
      continue;
    assert(i);
    ev->key.keysym.sym = i;
    return 1;
  }
  assert(0);
  return 0;
}
/*typedef union {
  uint8_t type;
  SDL_KeyboardEvent key;
  SDL_UserEvent user;
} SDL_Event;
*/
int SDL_WaitEvent(SDL_Event *event) {
  /*(int) Returns 1 on success or 0 if there was an error while
   * waiting for events; call SDL_GetError() for more information.*/
  do {
    SDL_PollEvent(event);
  } while (event->type == SDL_NONE);
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t *SDL_GetKeyState(int *numkeys) { return NULL; }
