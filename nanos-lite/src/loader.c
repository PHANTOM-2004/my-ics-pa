#include <common.h>
#include <elf.h>
#include <proc.h>
#include <stdint.h>

#ifdef __LP64__
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#endif

#if defined(__ISA_AM_NATIVE__)
#define EXPECT_TYPE EM_X86_64
#elif defined(__ISA_X86__)
#define EXPECT_TYPE EM_386 // see /usr/include/elf.h to get the right type

#elif defined(__ISA_RISCV32__)
#define EXPECT_TYPE EM_RISCV
#else
#error Unsupported ISA
#endif

extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;
size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);

typedef struct {
  bool is_elf;
  Elf_Ehdr elf_header;
  Elf_Phdr *program_header;
  Elf_Shdr section_header_entry1;

  size_t program_header_num;
  uintptr_t program_header_offset;
  uintptr_t entry_point;
} Elf_parser;

static void read_elf_header(Elf_parser *const _this) {
  ramdisk_read(&_this->elf_header, 0, sizeof(Elf_Ehdr));

  _this->is_elf =
      *(uint32_t *)_this->elf_header.e_ident == 0x464c457f; // magic number
  Log("ELF magic number correct: %x; Expected type %d",
      *(uint32_t *)_this->elf_header.e_ident, EXPECT_TYPE);
  assert(_this->is_elf);
  assert(_this->elf_header.e_machine == EXPECT_TYPE);

  _this->program_header_num = _this->elf_header.e_phnum;
  _this->program_header_offset = _this->elf_header.e_phoff;

  Log("e_phnum: %d  e_phoff: 0x%x", _this->program_header_num,
      _this->program_header_offset);
  Log("program header entry size: struct[%d], read[%d]", sizeof(Elf_Phdr),
      _this->elf_header.e_phentsize);

  assert(_this->elf_header.e_phentsize == sizeof(Elf_Phdr));
  
  _this->entry_point = _this->elf_header.e_entry;
  Log("Entry: 0x%x", _this->entry_point);
  assert(_this->entry_point);
}

static void read_program_header(Elf_parser *const _this) {
  _this->program_header =
      (Elf_Phdr *)malloc(_this->program_header_num * sizeof(Elf_Phdr));


  ramdisk_read(_this->program_header, _this->program_header_offset,
               sizeof(Elf_Phdr) * _this->program_header_num);
}

static void _destructor(Elf_parser *const _this) {
  // fclose(_this->fp);
  free(_this->program_header);
}

static int parse_elf(Elf_parser *_this, char const *const fname) {

  read_elf_header(_this);

  read_program_header(_this);

  return 0;
}

static uintptr_t loader(PCB *pcb, const char *filename) {
  Log("call loader");
  Elf_parser _this[1] = {{
      .is_elf = true,
      .program_header_num = 0,
  }};
  parse_elf(_this, filename);

  // TODO: we don't support large program_header_num now;
  assert(_this->program_header_num);

  for (size_t i = 0; i < _this->program_header_num; i++) {
    if (_this->program_header[i].p_type != PT_LOAD)
      continue;
    Elf_Phdr const *const p = _this->program_header + i;

    uint8_t *buffer = (uint8_t *)malloc(sizeof(uint8_t) * p->p_memsz);
    ramdisk_read(buffer, p->p_offset, p->p_filesz);
    assert(p->p_memsz >= p->p_filesz);
    memset(buffer + p->p_filesz, 0, p->p_memsz - p->p_filesz);

    memcpy((void *)(uintptr_t)p->p_vaddr, buffer, p->p_filesz);
    /*set to zero*/
    Log("load ELF to [0x%x, 0x%x)", p->p_vaddr, p->p_vaddr + p->p_filesz);
  }
  
  uintptr_t const res = _this->entry_point;

  _destructor(_this);

  return res;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = 0x%p", entry);
  ((void (*)())entry)();
}
