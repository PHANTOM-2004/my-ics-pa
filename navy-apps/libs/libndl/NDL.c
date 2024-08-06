#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;

#define FD_EVENT 3

uint32_t NDL_GetTicks() {
  extern int _gettimeofday(struct timeval * tv, struct timezone * tz);
  struct timeval tv[1];
  _gettimeofday(tv, NULL);
  return tv->tv_sec * 1000u + tv->tv_usec / 1000u;
}

int NDL_PollEvent(char *buf, int len) {
  int const fd = open("/dev/events", 0);
  return read(fd, buf, len);
}

void NDL_OpenCanvas(int *w, int *h) {
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w;
    screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0)
        continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0)
        break;
    }
    close(fbctl);
  }

  // parse the info we read
  char buf[64];
  int const fd = open("/proc/dispinfo", 0);
  read(fd, buf, sizeof(buf));
  sscanf(buf, "WIDTH:%d\nHEIGHT:%d\n", &screen_w, &screen_h);
  if (*w == 0 || *h == 0 || *w > screen_w || *h > screen_h)
    *w = screen_w, *h = screen_h;
  // printf("open: %d x %d\n", *w, *h);
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {

  for (int j = y; j < y + h; j++) {
    // each time we draw a line
    int const offset = j * screen_w + x;
    lseek(fbdev, offset, SEEK_SET);
    write(fbdev,pixels + j * w, w);
  }
}

void NDL_OpenAudio(int freq, int channels, int samples) {}

void NDL_CloseAudio() {}

int NDL_PlayAudio(void *buf, int len) { return 0; }

int NDL_QueryAudio() { return 0; }

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  fbdev = open("/dev/fb", 0);
  close(fbdev);
  return 0;
}

void NDL_Quit() {}
