#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int printf(const char *fmt, ...) { panic("Not implemented"); }

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
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

  int ret = 0;
  int val_int = 0;
  size_t cp_len = 0;
  char const *val_charptr = NULL;
  size_t str_len = 0;
#define BUFFER_SIZE 30
  static char buffer[BUFFER_SIZE] = "\0";
  char *pos = NULL;

  for (char const *p = fmt; *p; p++) {
    if (*p != '%') {
      *out++ = *p;
      ret++;
      continue;
    }

    p++;
    switch (*p) {
    case 'd':
      /*This is for int type*/
      val_int = va_arg(arguments, int);
      str_len = 0;
      pos = buffer + BUFFER_SIZE - 1;
      do {
        *pos = val_int % 10 + '0';
        val_int /= 10;
        str_len++;
        pos--;
      } while (val_int);
      memcpy(out, pos + 1, str_len);
      out += str_len;
      ret += str_len;
      break;

    case 's':
      val_charptr = va_arg(arguments, char const *);
      cp_len = strlen(val_charptr);
      memcpy(out, val_charptr, cp_len);
      ret += cp_len;
      out += cp_len;
      break;

    case '%':
      *out++ = '%';
      ret++;
      break;

    default:
      panic("Not implemented");
    }

  }
  *out = '\0'; // at last is a NULL terminator

  va_end(arguments);
#undef BUFFER_SIZE
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
