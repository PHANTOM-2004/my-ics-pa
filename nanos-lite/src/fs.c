#include <fs.h>

typedef size_t (*ReadFn)(void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn)(const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
} Finfo;

enum { FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB };

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
    [FD_STDIN] = {"stdin", 0, 0, invalid_read, invalid_write},
    [FD_STDOUT] = {"stdout", 0, 0, invalid_read, invalid_write},
    [FD_STDERR] = {"stderr", 0, 0, invalid_read, invalid_write},
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

  Log("Open [fd=%d], total [%d]", descriptor, FS_TABLE_LEN);
  assert(IS_FD_VALID(descriptor));
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
  Log("seek return 0x%x[%d]", file_pos[fd], file_pos[fd]);
  return file_pos[fd];
}

size_t fs_read(int fd, void *buf, size_t len) {
  /*read - read from a file descriptor
   * read() attempts to read up to count bytes from file descriptor fd into the
   * buffer starting at buf.*/

  /*here the file size is fixed, so the read should not go beyond the bound*/
  /*if it goes beyond, zero is returned */
  extern size_t ramdisk_read(void *buf, size_t offset, size_t len);

  Log("read file size: %d, current pos: %d", file_table[fd].size, file_pos[fd]);
  assert(IS_FD_VALID(fd));
  if (IS_STDIO(fd))
    return 0; // read nothing

  else if (file_pos[fd] > file_table[fd].size) {
    return 0;
  }

  else if (file_pos[fd] + len > file_table[fd].size) {
    // if it go beyond
    len = file_table[fd].size - file_pos[fd];
  }

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

  if (fd == FD_STDIN)
    return 0;

  else if (fd == FD_STDERR || fd == FD_STDOUT) {
    size_t ret = 0;
    for (size_t i = 0; i < len; i++) {
      putch(((char const *)buf)[i]);
      ret++;
    }
    return ret;
  }

  else if (file_pos[fd] > file_table[fd].size) {
    return 0;
  }

  else if (file_pos[fd] + len > file_table[fd].size) {
    len = file_table[fd].size - file_pos[fd];
  }

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
}
