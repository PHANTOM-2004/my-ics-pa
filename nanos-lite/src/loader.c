#include "elfp.h"
#include "fs.h"
#include "klib-macros.h"
#include "memory.h"
#include <proc.h>
#include <stdint.h>


extern uint8_t ramdisk_start;
extern uint8_t ramdisk_end;
size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);

uintptr_t loader(PCB *pcb, const char *filename) {
  Log("call loader");
  Elf_parser _this[1] = {{.is_elf = true,
                          .program_header_num = 0, //
                          .fd = fs_open(filename, 0, 0)}};
  parse_elf(_this);
  // TODO: we don't support large program_header_num now;
  assert(_this->program_header_num);

  for (size_t i = 0; i < _this->program_header_num; i++) {
    if (_this->program_header[i].p_type != PT_LOAD)
      continue;
    Elf_Phdr const *const p = _this->program_header + i;

    assert(p->p_memsz >= p->p_filesz);
    
    // NOTE: alloc by new_page
    uint8_t *buffer =
        (uint8_t *)new_page(DIVCEIL(sizeof(uint8_t) * p->p_filesz, PGSIZE));
    fs_lseek(_this->fd, p->p_offset, SEEK_SET);
    fs_read(_this->fd, buffer, p->p_filesz); // read files_size

    memcpy((void *)(uintptr_t)p->p_vaddr, buffer, p->p_filesz);

    uintptr_t const zero_begin =
        ((uintptr_t)p->p_paddr + (uintptr_t)p->p_filesz);
    uintptr_t const zero_end = (uintptr_t)p->p_paddr + (uintptr_t)p->p_memsz;
    memset((void *)zero_begin, 0,
           zero_end - zero_begin); // set to zero
    //
    Log("set zero [0x%x, 0x%x)", zero_begin, zero_end);
    Log("check [0x%x] %x", zero_begin, *(uint32_t *)zero_begin);

    // My immplementation before is that i alloc a buffer with size
    // filesize, and set the last part zero, but i forget to copy the zero
    // part. surely I wrote `copy size filesize`, but memsz >= filesize; It
    // is I who does not set the memory zero ! memcpy((void
    // *)(uintptr_t)p->p_vaddr, buffer, p->p_filesz);
    /*set to zero*/
    Log("load ELF to [0x%x, 0x%x)", p->p_vaddr, p->p_vaddr + p->p_filesz);
  }

  uintptr_t const res = _this->entry_point;

  _destructor(_this);

  assert(res);
  return res;
}

void naive_uload(PCB *pcb, const char *filename) {
  Log("Calling loader");
  uintptr_t entry = loader(pcb, filename);
  // we need to set user sp
  Log("Jump to entry = 0x%p", entry);
  ((void (*)())entry)();
}
