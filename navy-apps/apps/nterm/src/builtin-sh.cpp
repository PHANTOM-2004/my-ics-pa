#include <SDL.h>
#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() { sh_printf("sh> "); }

static void sh_handle_cmd(const char *cmd) {
  // parse the argument here
  // now treat the cmd as file name

  /* The  setenv()  function  adds  the  variable  name  to  the environment
   with
   * the value value, if name does not already exist.  If name does exist in the
   * environment, then its value is changed to value if overwrite is
       nonzero; if overwrite is zero, then the value of name is not changed
       (and setenv() returns a success status).
    This function makes copies of the strings pointed to by name and value (by
   contrast with putenv(3)).*/
  setenv("PATH", "/bin:/usr/bin", 0);

  /*The environment of the new process image is specified via the argument envp.
   *The envp argument is an array of pointers to null-terminated strings and
   must be terminated by a null pointer.*/
  static char buf[1024] = "\0";
  static char *argv[128] = {NULL};

  strncpy(buf, cmd, sizeof(buf) - 1);
  char *p = buf;

  while (*p && *p != '\n')
    p++; // trim the \n
  *p = '\0';

  int cnt = 0;
  p = strtok(buf, " "); // this time get filename
  argv[cnt++] = p;
  assert(argv[0]);

  while ((p = strtok(NULL, " \t")) != NULL) {
    assert(cnt < sizeof(argv) / sizeof(argv[0]));
    argv[cnt] = p;
    assert(argv[cnt]);
    cnt++;
  }

  assert(cnt < sizeof(argv) / sizeof(argv[0]));
  argv[cnt] = NULL;

  int const ret = execvp(argv[0], argv);
  assert(ret != -1);
}

void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
