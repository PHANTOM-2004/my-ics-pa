#pragma once

#ifndef __ELF_PARSER_H
#define __ELF_PARSER_H

#include "common.h"
#include <elf.h>

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

typedef struct {
  bool is_elf;
  Elf_Ehdr elf_header;
  Elf_Phdr *program_header;
  Elf_Shdr section_header_entry1;

  size_t program_header_num;
  uintptr_t program_header_offset;
  uintptr_t entry_point;
  int const fd;
} Elf_parser;

void _destructor(Elf_parser *const _this);
int parse_elf(Elf_parser *_this);
uintptr_t get_elf_entry(char const *const fname);

#endif
