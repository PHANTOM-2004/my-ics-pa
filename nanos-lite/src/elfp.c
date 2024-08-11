#include "elfp.h"
#include "fs.h"
#include <stdint.h>

static bool check_elf_magic(Elf_Ehdr const * const elf_header) {
  bool res = *(uint32_t *)elf_header->e_ident == 0x464c457f; // magic number
  Log("ELF magic number correct: %x; Expected type %d",
      *(uint32_t *)elf_header->e_ident, EXPECT_TYPE);
  assert(res);

  res = elf_header->e_machine == EXPECT_TYPE;
  assert(res);

  return res;
}

static void read_elf_header(Elf_parser *const _this) {
  fs_lseek(_this->fd, 0, SEEK_SET);
  fs_read(_this->fd, &_this->elf_header, sizeof(Elf_Ehdr));
  // ramdisk_read(&_this->elf_header, 0, sizeof(Elf_Ehdr));

  _this->is_elf = check_elf_magic(&_this->elf_header);

  _this->program_header_num = _this->elf_header.e_phnum;
  _this->program_header_offset = _this->elf_header.e_phoff;

  Log("e_phnum: %d  e_phoff: 0x%x", _this->program_header_num,
      _this->program_header_offset);
  Log("program header entry size: struct[%d], read[%d]", sizeof(Elf_Phdr),
      _this->elf_header.e_phentsize);

  assert(_this->elf_header.e_phentsize == sizeof(Elf_Phdr));

  _this->entry_point = _this->elf_header.e_entry;
  Log("Entry: 0x%x", (unsigned)_this->entry_point);
  assert(_this->entry_point);
}

static void read_program_header(Elf_parser *const _this) {
  _this->program_header =
      (Elf_Phdr *)malloc(_this->program_header_num * sizeof(Elf_Phdr));

  fs_lseek(_this->fd, _this->program_header_offset, SEEK_SET);
  fs_read(_this->fd, _this->program_header,
          sizeof(Elf_Phdr) * _this->program_header_num);
  // ramdisk_read(_this->program_header, _this->program_header_offset,
  //             sizeof(Elf_Phdr) * _this->program_header_num);
}

void _destructor(Elf_parser *const _this) {
  // fclose(_this->fp);
  free(_this->program_header);
  fs_close(_this->fd);
}

int parse_elf(Elf_parser *_this) {

  read_elf_header(_this);

  read_program_header(_this);

  return 0;
}

uintptr_t get_elf_entry(char const *const fname) {
  Elf_Ehdr header;
  uintptr_t entry = 0;

  int const fd = fs_open(fname, 0, 0);
  fs_lseek(fd, 0, SEEK_SET);
  fs_read(fd, &header, sizeof(header) * 1);

  check_elf_magic(&header);
  
  entry = header.e_entry;
  Log("get entry: 0x%x", entry);

  return entry;
}
