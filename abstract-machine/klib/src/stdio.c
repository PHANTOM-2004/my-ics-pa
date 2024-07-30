#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

#define STRING_BUFFER_SIZE 30

typedef void (*string_handler_t)(char *, char const *, size_t);

static inline void str_write_to_buffer(char *out, char const *in, size_t size) {
  assert(out);
  assert(in);
  memcpy(out, in, size);
}

static inline void str_write_to_stdout(char *out, char const *in, size_t size) {
  assert(out == NULL); // out is stdout
  assert(in);
  size_t cnt = 0;
  while (*in && cnt++ < size)
    putch(*in++);
}

static inline bool is_placeholder(char const ch) {
  return ch == 'd' || ch == 's' || ch == 'f';
}

int printf_base(char *out, char const *fmt, size_t const n,
                string_handler_t shandler, va_list ap) {
  // TODO: add bit width function, and better logic
  int ret = 0;
  int val_int = 0;
  char val_char = 0;
  size_t cnt = 0;
  char buffer[STRING_BUFFER_SIZE] = "\0";
  char *buf_pos;

  char const *src_pos = NULL;
  size_t src_length = 0;
  char filler = ' ';
  int width = 0;

  bool go_to_switch = false;

  for (char const *p = fmt; cnt <= n && *p; p++) {
    if (*p != '%' && !go_to_switch) {
      src_pos = p;
      src_length = 1;

      if (cnt + src_length > n)
        src_length = n - cnt;

      shandler(out, src_pos, src_length);
      if (out)
        out += src_length;
      ret += src_length, cnt += src_length;
      continue;
    }

    go_to_switch = false;
    p++;
    switch (*p) {
    case 'd':
      /*This is for int type*/
      val_int = va_arg(ap, int);
      src_length = 0;
      buf_pos = buffer + STRING_BUFFER_SIZE - 1;
      do {
        *buf_pos = val_int % 10 + '0';
        val_int /= 10;
        src_length++;
        buf_pos--;
      } while (val_int);

      src_pos = buf_pos + 1;
      break;

    case 's':
      src_pos = va_arg(ap, char const *);
      src_length = strlen(src_pos);
      break;

    case 'c':
      val_char = (char)va_arg(ap, int);
      src_pos = &val_char;
      src_length = 1;
      break;

    case '%':
      src_pos = p;
      src_length = 1;
      break;

    case '0': // it is flag after '%'
      go_to_switch = true;
      filler = '0';           // if not just ignore witdh = 0
      width = *(p + 1) - '0'; // TODO: simply suppose it one bit width
      continue;

      break;

    default: // not %

      if (*p > '0' && *p <= '9') {
        width = *p - '0';
        p--;
        go_to_switch = true;
        continue;
      }

      printf("%s\n", fmt);
      panic("Not implemented");
    }
    // now write to buffer/stdout

    if (cnt + src_length > n)
      src_length = n - cnt;

    if (src_length < width)
      for (int i = 0; i < width - src_length; i++)
        shandler(out, &filler, 1);

    shandler(out, src_pos, src_length);

    int const step = src_length < width ? width : src_length;
    if (out)
      out += step;
    ret += step, cnt += step;
  }
  if (out)
    *out = '\0'; // at last is a NULL terminator

  assert((size_t)ret <= n);
  return ret;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  assert(out);
  assert(fmt);
  int const ret = printf_base(out, fmt, -1, str_write_to_buffer, ap);
  return ret;
}

int printf(const char *fmt, ...) {
  va_list arguments;
  va_start(arguments, fmt);
  int const ret = printf_base(NULL, fmt, -1, str_write_to_stdout, arguments);
  va_end(arguments);
  return ret;
}

int sprintf(char *out, const char *fmt, ...) {
  // TODO: only %s and %d implemented

  /*Upon successful return, these functions return the number of bytes printed
     (excluding the null byte used to end output to strings).

       The functions snprintf() and vsnprintf() do not write more than size
     bytes (including the terminating null byte ('\0')).  If the output was
     truncated due to this limit, then the return value is the number of
     characters (excluding  the  terminating  null byte) which would have been
     written to the final string if enough space had been available.  Thus, a
     return value of size or more means that the output was truncated.  (See
     also below under CAVEATS.)

       If an output error is encountered, a negative value is returned.
  */

  /*	sprintf(buf, "%s", "Hello world!\n");
        check(strcmp(buf, "Hello world!\n") == 0);

        sprintf(buf, "%d + %d = %d\n", 1, 1, 2);
        check(strcmp(buf, "1 + 1 = 2\n") == 0);

        sprintf(buf, "%d + %d = %d\n", 2, 10, 12);
        check(strcmp(buf, "2 + 10 = 12\n") == 0);
   */
  assert(out);
  assert(fmt);

  va_list arguments;
  va_start(arguments, fmt);
  int const ret = printf_base(out, fmt, -1, str_write_to_buffer, arguments);
  va_end(arguments);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  assert(out);
  assert(fmt);
  va_list arguments;
  va_start(arguments, fmt);
  int const ret = printf_base(out, fmt, n, str_write_to_buffer, arguments);
  va_end(arguments);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  assert(out);
  assert(fmt);
  int const ret = printf_base(out, fmt, n, str_write_to_buffer, ap);
  return ret;
}

#endif
