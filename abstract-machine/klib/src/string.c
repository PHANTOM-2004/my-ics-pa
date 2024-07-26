#include "am.h"
#include <klib-macros.h>
#include <klib.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  assert(s);
  size_t len = 0;
  while (*s) {
    s++;
    len++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  assert(dst);
  assert(src);
  char *res = dst;

  while (*src)
    *dst++ = *src++;
  *dst = '\0';

  return res;
}

char *strncpy(char *dst, const char *src, size_t n) {
  assert(dst);
  assert(src);
  char *res = dst;
  size_t i = 0;

  while (*src && i++ < n)
    *dst++ = *src++;

  for (i = 0; i < n; i++)
    *dst++ = '\0';

  return res;
}

char *strcat(char *dst, const char *src) {
  assert(src);
  assert(dst);
  char *res = dst;

  while (*dst)
    dst++;

  while (*src)
    *dst++ = *src++;

  *dst = '\0';
  return res;
}

int strcmp(const char *s1, const char *s2) {
  assert(s1);
  assert(s2);
  while (*s1 && *s2) {
    if (*s1 < *s2)
      return -1;
    else if (*s1 > *s2)
      return 1;
    s1++,s2++;
  }

  if (*s1)
    return 1;
  else if (*s2)
    return -1;

  return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  return memcmp(s1, s2, n);
}

void *memset(void *s, int c, size_t n) {
  for (size_t i = 0; i < n; i++)
    *((uint8_t *)s + i) = (uint8_t)c;
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  assert(src);
  assert(dst);

  uint32_t *buffer = (uint32_t *)malloc(n / 4 + 1);
  memcpy(buffer, src, n);
  memcpy(dst, buffer, n);
  free(buffer);
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  assert(out);
  assert(in);

  for (size_t i = 0; i < n; i++)
    *((uint8_t *)in + i) = *((uint8_t *)out + i);
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  assert(s1);
  assert(s2);

  for (size_t i = 0; i < n; i++) {
    unsigned char const _l = *(unsigned char *)s1;
    unsigned char const _r = *(unsigned char *)s2;
    if (_l < _r)
      return -1;
    else if (_l > _r)
      return 1;
    s1++,s2++;
  }
  return 0;
}

#endif
