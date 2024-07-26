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
  return 0;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
