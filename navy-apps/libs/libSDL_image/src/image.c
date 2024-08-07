#include "sdl-video.h"
#include <stdint.h>
#include <stdlib.h>
#define SDL_malloc malloc
#define SDL_free free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}

SDL_Surface *IMG_Load(const char *filename) {
  /* 1. open file with libc, and get file size
   * 2. malloc size buffer
   * 3. read file to buffer
   * 4. call STBIMG_LoadFromMemory, which return a pointer
   * 5. close file, free buffer
   * 6. return pointer
   * */
  FILE *_image = fopen(filename, "rb");
  assert(_image);
  fseek(_image, 0, SEEK_END);
  size_t const file_size = ftell(_image);

  uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * file_size);
  assert(buffer);

  fseek(_image, 0, SEEK_SET);
  fread(buffer, sizeof(uint8_t), file_size, _image);

  SDL_Surface *const res =
      STBIMG_LoadFromMemory((unsigned char const *)buffer, file_size);

  free(buffer);
  fclose(_image);

  return res;
}

int IMG_isPNG(SDL_RWops *src) { return 0; }

SDL_Surface *IMG_LoadJPG_RW(SDL_RWops *src) { return IMG_Load_RW(src, 0); }

char *IMG_GetError() { return "Navy does not support IMG_GetError()"; }
