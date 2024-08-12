#include "amdev.h"
#include "log.h"
#include <fs.h>
#include <stdint.h>
#include <errno.h>

typedef size_t (*ReadFn)(void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn)(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
} Finfo;

enum { FD_STDIN, FD_STDOUT, FD_STDERR, FD_EVENTS, FD_DISP, FD_FB };

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

extern size_t serial_write(void const *buf, size_t offset, size_t len);
extern size_t events_read(void *buf, size_t offset, size_t len);
extern size_t fb_write(const void *buf, size_t offset, size_t len);
extern size_t dispinfo_read(void *buf, size_t offset, size_t len);

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
    [FD_STDIN] = {"stdin", 0, 0, invalid_read, invalid_write},
    [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
    [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
    [FD_EVENTS] = {"/dev/events", 0, 0, events_read, invalid_write},
    [FD_DISP] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write},
    [FD_FB] = {"/dev/fb", 0, 0, invalid_read, fb_write},
#include "files.h"
};

#define FS_TABLE_LEN (sizeof(file_table) / sizeof(file_table[0]))
#define IS_STDIO(fd) ((fd) >= 0 && (fd) <= 2)
#define IS_FD_VALID(fd) ((fd) >= 0 && (fd) < FS_TABLE_LEN)

static size_t file_pos[FS_TABLE_LEN] = {};

int fs_open(const char *pathname, int flags, int mode) {
  /*if fs_open does not find the file, it should call assert(0)*/
  /*we ignore flags and mode*/
  int descriptor = -1;
  for (int i = 0; i < FS_TABLE_LEN; i++) {
    if (strcmp(pathname, file_table[i].name))
      continue;
    descriptor = i;
    break;
  }

  // Log("Open [fd=%d], total [%d]", descriptor, FS_TABLE_LEN);
  if (!IS_FD_VALID(descriptor)) {
    Log("%s[%s]", ANSI_FMT("FILE NOT EXISTS", ANSI_FG_RED), pathname);
    // assert(IS_FD_VALID(descriptor));
    return -ENOENT;
  }
  file_pos[descriptor] = 0; // when open the pos is set to zero
  /*The  return  value  of open() is a file descriptor, a small,
   * nonnegative integer that is an index to an entry in the process's
   * table of open file descriptors.*/
  return descriptor;
}

/*
 * Except for putch for STDOUT,STDERR, the operation on file_table
 * STDIN,STDOUT,STDERR is ignored now
 * */

size_t fs_lseek(int fd, size_t offset, int whence) {
  /*lseek() allows the file offset to be set beyond the end of the file
   * (but this does not change the size of the file).  If data is later
   * written at this point, subsequent reads of the data in the gap (a "hole")
   * return null bytes ('\0') until data is actually written into the gap.*/
  assert(IS_FD_VALID(fd));
  if (IS_STDIO(fd))
    return 0;
  switch (whence) {
  case SEEK_CUR:
    /*The file offset is set to its current location plus offset bytes*/
    file_pos[fd] += offset; // cur pos + offset
    break;
  case SEEK_END:
    /*The file offset is set to the size of the file plus offset bytes.*/
    file_pos[fd] = offset + file_table[fd].size;
    break;
  case SEEK_SET:
    /*The file offset is set to offset bytes.*/
    file_pos[fd] = offset;
    break;
  }

  if (file_pos[fd] > file_table[fd].size) {
    assert(0);
    return -1;
  }
  /* Upon successful completion, lseek() returns the resulting offset location
   * as measured in bytes from the beginning of the file.  On error, the value
   * (off_t) -1 is returned and errno is set to indicate the error.*/
  // Log("seek return %x [%d]", file_pos[fd], file_pos[fd]);
  return file_pos[fd];
}

size_t fs_read(int fd, void *buf, size_t len) {
  /*read - read from a file descriptor
   * read() attempts to read up to count bytes from file descriptor fd into the
   * buffer starting at buf.*/

  /*here the file size is fixed, so the read should not go beyond the bound*/
  /*if it goes beyond, zero is returned */
  extern size_t ramdisk_read(void *buf, size_t offset, size_t len);

  // Log("read file name %s, size: %d, current pos: %d", file_table[fd].name,
  //    file_table[fd].size, file_pos[fd]);
  assert(IS_FD_VALID(fd));

  if (file_table[fd].read != NULL)
    return file_table[fd].read(buf, file_table[fd].disk_offset + file_pos[fd],
                               len);

  else if (file_pos[fd] > file_table[fd].size)
    return 0;

  else if (file_pos[fd] + len > file_table[fd].size)
    // if it go beyond
    len = file_table[fd].size - file_pos[fd];

  ramdisk_read(buf, file_table[fd].disk_offset + file_pos[fd], len);
  file_pos[fd] += len;

  /*On files that support seeking, the read operation commences at the file
   * offset, and the file offset is incremented by the number of bytes read.  If
   * the file offset is at or past the end of file, no bytes are read, and
   * read() returns zero.*/

  /* On  success,  the number of bytes read is returned
   * (zero indicates end of file), and the file position
   * is advanced by this number.
   * On error, -1 is returned, and errno is set to indicate the error.
   * */
  // Log("read first 4bytes: 0x%x[%d]", *(uint32_t *)buf, *(uint32_t *)buf);
  return len;
}

size_t fs_write(int fd, const void *buf, size_t len) {

  // sys write need to ret correct value;
  // On success, the number of bytes written is returned.
  // On error, -1 is returned, and errno is set to indicate the error.
  extern size_t ramdisk_write(const void *buf, size_t offset, size_t len);

  assert(IS_FD_VALID(fd));

  if (file_table[fd].write != NULL)
    return file_table[fd].write(buf, file_table[fd].disk_offset + file_pos[fd],
                                len);

  // pointer > size
  else if (file_pos[fd] > file_table[fd].size)
    return 0;

  else if (file_pos[fd] + len > file_table[fd].size)
    len = file_table[fd].size - file_pos[fd];

  Log("write file size: %d, current pos: %d", file_table[fd].size,
      file_pos[fd]);

  ramdisk_write(buf, file_table[fd].disk_offset + file_pos[fd], len);
  file_pos[fd] += len;
  return len;
}

int fs_close(int fd) {
  /*fs_close always return zero*/
  return 0;
}
void init_fs() {
  // TODO: initialize the size of /dev/fb
  AM_GPU_CONFIG_T const gconfig = io_read(AM_GPU_CONFIG);
  uint32_t const w = gconfig.width;
  uint32_t const h = gconfig.height;
  file_table[FD_FB].size = w * h * sizeof(uint32_t);
  file_table[FD_FB].disk_offset = 0; // suppose is zero
}
