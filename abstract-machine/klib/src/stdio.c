#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

#define STRING_BUFFER_SIZE 30
#define STDOUT_BUFFER_SIZE 10

#define min(a, b) (a) < (b) ? (a) : (b)

enum {
  LONG_TYPE,
  LONG_LONG_TYPE,
  NONE_TYPE,
};

typedef struct {
  size_t width;
  size_t buffer_filled_len;
  size_t total_out_len;
  size_t const max_out_len;

  size_t cur_out_len;
  char filler;
  bool is_conversion;
  bool is_flag;

  char const *cur_out_str;
  char *const out_dst;

  char str_buffer[STRING_BUFFER_SIZE];
  int64_t val_int;
  uint64_t val_uint;
  char val_char;
  uintptr_t val_uptr;
  int int_type;

} printfParser;

typedef void (*buffer_handler_t)(printfParser *const _this);

static void flush_buffer(printfParser *const _this) {
  for (size_t i = 0; i < _this->buffer_filled_len; i++)
    putch(_this->out_dst[i]);
  _this->buffer_filled_len = 0;
}

static inline bool is_buffer_full(printfParser const *const _this) {
  return _this->buffer_filled_len >= STDOUT_BUFFER_SIZE - 1;
}

static inline bool reach_maxlen(printfParser const *const _this) {
  return _this->total_out_len > _this->max_out_len;
}

static inline void fill_buffer(printfParser *const _this, char const ch) {
  _this->out_dst[_this->buffer_filled_len++] = ch;
}

static inline void str_write_to_buffer(printfParser *const _this) {
  assert(_this->out_dst);
  assert(_this->cur_out_str);

  if (_this->cur_out_len < _this->width) {
    size_t const fill_len = _this->width - _this->cur_out_len;
    for (size_t i = 0; !reach_maxlen(_this) && i < fill_len; i++) {
      _this->out_dst[_this->total_out_len++] = _this->filler;
    }
  }

  size_t const cp_len = reach_maxlen(_this)
                            ? 0
                            : (min(_this->max_out_len - _this->total_out_len,
                                   _this->cur_out_len));
  memcpy(_this->out_dst + _this->total_out_len, _this->cur_out_str, cp_len);
  _this->total_out_len += cp_len;
}

static void str_write_to_stdout(printfParser *const _this) {
  assert(_this->out_dst);
  assert(_this->cur_out_str);
  size_t cnt = 0;
  char const *p = _this->cur_out_str;

  //
  if (_this->cur_out_len < _this->width) {
    size_t const fill_len = _this->width - _this->cur_out_len;
    for (size_t i = 0; !reach_maxlen(_this) && i < fill_len; i++) {
      if (is_buffer_full(_this))
        flush_buffer(_this);
      fill_buffer(_this, _this->filler);
      _this->total_out_len++;
    }
  }

  while (*p && !reach_maxlen(_this) && cnt < _this->cur_out_len) {
    // when the buffer is filled
    if (is_buffer_full(_this))
      flush_buffer(_this);

    // write to buffer
    fill_buffer(_this, *p);

    // if there is a '\n'
    if (*p == '\n')
      flush_buffer(_this);

    _this->total_out_len++;
    cnt++;
    p++;
  }
}

static inline bool is_conversion(char const ch) {
  // these type are trivial integer
  return ch == 'd' || ch == 's' || ch == 'u' || ch == 'x' || ch == 'p' ||
         ch == 'f' || ch == 'c';
}

static inline bool is_flag(char const ch) { return ch == '0'; }

static inline int itohex(int const num, bool const upper) {
  assert(num >= 0 && num < 16);
  if (num < 10)
    return num + '0';
  return num - 10 + (upper ? 'A' : 'a');
}

// return the length
static void _itoa(printfParser *const _this) {
  int length = 0;
  char *buf_pos = _this->str_buffer + STRING_BUFFER_SIZE - 1;
  bool const negative = _this->val_int < 0;
  if (negative)
    _this->val_int = -_this->val_int;

  do {
    *buf_pos = _this->val_int % 10 + '0';
    assert(isdigit(*buf_pos));
    _this->val_int /= 10;
    length++;
    buf_pos--;
  } while (_this->val_int);

  if (negative) {
    *buf_pos = '-';
    buf_pos--;
    length++;
  }

  _this->cur_out_len = length;
  _this->cur_out_str = buf_pos + 1;
}

static void _utoa(printfParser *const _this, unsigned const base,
                  bool const upper) {
  int length = 0;
  char *buf_pos = _this->str_buffer + STRING_BUFFER_SIZE - 1;
  do {
    *buf_pos = (char)itohex(_this->val_uint % base, upper);
    _this->val_uint /= base;
    length++;
    buf_pos--;
  } while (_this->val_uint);

  _this->cur_out_len = length;
  _this->cur_out_str = buf_pos + 1;
}

static char const *parse_format(printfParser *const parser, char const *p) {
  if (*p != '%') {
    parser->cur_out_str = p;
    parser->cur_out_len = 1;
    // NOTE: reset here
    parser->width = 0;
    parser->filler = ' ';
    p++;
  } else {
    // parser things after %
    p++;
    assert(*p); // else invalid expression

    // now read int type
    if (*p == 'l') {
      p++;
      parser->int_type = LONG_TYPE;
    }

    if (*p == 'l') {
      p++;
      parser->int_type = LONG_LONG_TYPE;
    }

    // if conversion
    parser->is_conversion = is_conversion(*p);
    parser->width = 0;
    if (parser->is_conversion) {
      return p;
    }

    // now read flag
    parser->is_flag = is_flag(*p);
    parser->filler = ' ';
    if (parser->is_flag) {
      parser->filler = *p; // TODO: only consider filler
      p++;
    }

    // now read witdh
    printf("%s", p);
    assert(*p && isdigit(*p));
    parser->width = atoi(p);

    while (!is_conversion(*p)) {
      assert(isdigit(*p));
      p++;
    }

    // then it is conversion
    parser->is_conversion = true; // p point to conversion
  }

  return p;
}

static int _printf_base(char *out, char const *fmt, size_t const n,
                        buffer_handler_t bhandler, va_list ap) {
  printfParser parser[1] = {{
      .width = 0,
      .filler = ' ',
      .buffer_filled_len = 0,
      .is_conversion = false,
      .is_flag = false,
      .total_out_len = 0,
      .max_out_len = n,
      .cur_out_len = 0,
      .cur_out_str = NULL,
      .out_dst = out,
      .int_type = NONE_TYPE,
  }};

  /*const char *T = "CALL KLIB PRINTF\n";
  while (*T)
    putch(*T++);*/

  for (char const *p = fmt; *p;) {
    if (parser->is_conversion) {
      parser->is_conversion = false;
      // parse conversion
      switch (*p) {
      case 'd':
        /*This is for int type*/
        if (parser->int_type == LONG_TYPE)
          parser->val_int = va_arg(ap, long int);
        else if (parser->int_type == LONG_LONG_TYPE)
          parser->val_int = va_arg(ap, long long int);
        else
          parser->val_int = va_arg(ap, int);

        _itoa(parser);
        break;

      case 's':
        parser->cur_out_str = va_arg(ap, char const *);
        parser->cur_out_len = strlen(parser->cur_out_str);
        break;

      case 'c':
        parser->val_char = (char)va_arg(ap, int);
        parser->cur_out_str = &parser->val_char;
        parser->cur_out_len = 1;
        break;

      case 'u':
        if (parser->int_type == LONG_TYPE)
          parser->val_uint = va_arg(ap, unsigned long int);
        else if (parser->int_type == LONG_LONG_TYPE)
          parser->val_uint = va_arg(ap, unsigned long long int);
        else
          parser->val_uint = va_arg(ap, unsigned int);

        _utoa(parser, 10, 0);
        break;

      case 'x':
      case 'X':
        parser->val_uint = va_arg(ap, unsigned int);
        _utoa(parser, 16, *p == 'X');
        break;
      case 'p':
      case 'P':
        parser->val_uptr = va_arg(ap, uintptr_t);
        _utoa(parser, 16, *p == 'P');
        break;

      default: // not %
        putch('\n');
        putch(*p);
        putch('\n');
        panic("Not implemented");
      }
      // go to next
      parser->int_type = NONE_TYPE;
      p++;
    } else {
      p = parse_format(parser, p);
      if (parser->is_conversion)
        continue;
    }
    // write to dst
    bhandler(parser);
  }

  flush_buffer(parser);
  return (int)parser->total_out_len;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  assert(out);
  assert(fmt);
  int const ret = _printf_base(out, fmt, -1, str_write_to_buffer, ap);
  out[ret] = '\0';
  return ret;
}

int printf(const char *fmt, ...) {
  va_list arguments;
  va_start(arguments, fmt);
  char buffer[STDOUT_BUFFER_SIZE];
  int const ret = _printf_base(buffer, fmt, -1, str_write_to_stdout, arguments);
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
  int const ret = _printf_base(out, fmt, -1, str_write_to_buffer, arguments);
  out[ret] = '\0';
  va_end(arguments);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  assert(out);
  assert(fmt);
  va_list arguments;
  va_start(arguments, fmt);
  int const ret = _printf_base(out, fmt, n - 1, str_write_to_buffer, arguments);
  out[n] = '\0';
  va_end(arguments);
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  assert(out);
  assert(fmt);
  int const ret = _printf_base(out, fmt, n - 1, str_write_to_buffer, ap);
  out[n] = '\0';
  return ret;
}

#endif
