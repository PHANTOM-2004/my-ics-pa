/*
 * created by YT Chen
 * 2024-7-27
 * parsing elf
 *
 */

#include "common.h"

#ifdef CONFIG_FTRACE
#include <elf.h>
#include <stdint.h>
#include <trace/ftrace.h>

typedef struct {
  bool is_elf32;
  Elf32_Ehdr elf_header;
  Elf32_Shdr *elf_section_header_table;
  Elf32_Sym *elf_symbol_table;
  char *elf_string_table;

  uint32_t symbol_table_addr;
  uint32_t symbol_table_offset;
  uint32_t symbol_table_size;
  uint32_t symbol_table_num;

  uint32_t string_table_addr;
  uint32_t string_table_offset;
  uint32_t string_table_size;
  uint32_t section_header_offset;
  uint32_t section_header_num;
  uint32_t section_header_size;

} Elf32_parser;

char *elf_file = NULL;
static FILE *fp = NULL;

/*elf functions*/
static int function_num = 0;
static Elf32_func
    all_functions[CONFIG_FTRACE_FUNC_MAX_NUM]; // support at most 128

Elf32_func const *get_elffunction(vaddr_t const target) {
  // uint32_t l = 0, r=0;
  for (int i = 0; i < function_num; i++) {
    if (target < all_functions[i].value ||
        target >= all_functions[i].value + all_functions[i].size)
      continue;

    // l = all_functions[i].value;
    // r = all_functions[i].value + all_functions[i].size;

    return all_functions + i;
  }

  Log("[FUNCTION NOT FOUND]target addr: %x does not belong to any function",
      target);
  return NULL;
}

static void check_malloc(void const *ptr) {
  if (!ptr) {
    Log("Malloc failed");
    assert(0);
  }
}

static int log_all_elf_functions() {
  for (int i = 0; i < function_num; i++) {
    Log("\tValue: %08x  Size: %08x Name: %s", all_functions[i].value,
        all_functions[i].size, all_functions[i].name);
  }
  return function_num;
}

static int get_function_name(Elf32_parser const *const _this,
                             uint32_t const str_idx) {
  /*we have the string table buffer*/
  fseek(fp, str_idx + _this->string_table_offset, SEEK_SET);
  strncpy(all_functions[function_num].name, _this->elf_string_table + str_idx,
          CONFIG_FTRACE_FUNC_NAME_LIMIT);
  all_functions[function_num].name[CONFIG_FTRACE_FUNC_NAME_LIMIT] = '\0';
  return 0;
}

static int log_funcs(Elf32_parser const *const _this) {
  /*count how many functions and malloc*/
  Log("read symbol table to catch functions");
  for (uint32_t i = 0; i < _this->symbol_table_num; i++) {
    if (ELF32_ST_TYPE(_this->elf_symbol_table[i].st_info) != STT_FUNC)
      continue;

    Assert(function_num < CONFIG_FTRACE_FUNC_MAX_NUM,
           "at most support [%d] functions", CONFIG_FTRACE_FUNC_MAX_NUM);

    Log("%2d: Value: %08x  Size:%4u  Type: %2u  Other: %2u str_idx: \t%u", i,
        _this->elf_symbol_table[i].st_value, _this->elf_symbol_table[i].st_size,
        ELF32_ST_TYPE(_this->elf_symbol_table[i].st_info),
        _this->elf_symbol_table[i].st_other,
        _this->elf_symbol_table[i].st_name);

    /*record value, size , name index*/
    all_functions[function_num].value = _this->elf_symbol_table[i].st_value;
    all_functions[function_num].size = _this->elf_symbol_table[i].st_size;
    uint32_t str_index = _this->elf_symbol_table[i].st_name;
    /* read string(name) of function here*/
    get_function_name(_this, str_index);

    function_num++;
  }

  Assert(function_num, "there must be FUNC type");
  Log("Get [%u] %s from ELF file", function_num,
      ANSI_FMT("FUNCTIONs", ANSI_FG_GREEN));
  log_all_elf_functions();

  return function_num;
}

static void read_elf_header(Elf32_parser *const _this) {
  fseek(fp, 0, SEEK_SET);
  fread(&_this->elf_header, sizeof(Elf32_Ehdr), 1, fp);
  _this->section_header_offset = _this->elf_header.e_shoff;
  _this->section_header_num = _this->elf_header.e_shnum;
  _this->section_header_size = _this->elf_header.e_shentsize;
  /*The ELF header's e_shoff member gives the byte offset from  the
   * beginning of the file to the section header table.
   * e_shnum holds the number of entries the section header table contains.
   * e_shentsize holds the size in bytes of each entry.
   * */

  _this->is_elf32 =
      _this->is_elf32 && (sizeof(Elf32_Shdr) == _this->section_header_size);
  Assert(_this->is_elf32, "not elf32 file");
}

static void read_section_header_table(Elf32_parser *const _this) {
  /*read section header table*/
  /*traverse the section header table , and find where the symbol table and
   * string table is */
  fseek(fp, _this->section_header_offset, SEEK_SET);
  _this->elf_section_header_table =
      (Elf32_Shdr *)malloc(sizeof(Elf32_Shdr) * _this->section_header_num);
  check_malloc(_this->elf_section_header_table);
  fread(_this->elf_section_header_table, sizeof(Elf32_Shdr),
        _this->section_header_num, fp);
}

static void locate_symbol_and_string_table(Elf32_parser *const _this) {
  bool find_symbol_table = false, find_string_table = false;
  // it means that _this->section_header_num is stored in section header table
  bool const seek_sh_num = !_this->section_header_num;
  if (seek_sh_num)
    _this->section_header_num = 1;
  for (uint32_t i = 0; i < _this->section_header_num; i++) {
    Log("section header[%2d] Addr: %08x", i,
        _this->elf_section_header_table[i].sh_addr);

    if (seek_sh_num && !i) {
      _this->section_header_num = _this->elf_section_header_table[0].sh_size;
      continue;
    }

    switch (_this->elf_section_header_table[i].sh_type) {

    case SHT_SYMTAB:

      _this->symbol_table_addr = _this->elf_section_header_table[i].sh_addr;
      _this->symbol_table_offset = _this->elf_section_header_table[i].sh_offset;
      _this->symbol_table_size = _this->elf_section_header_table[i].sh_size;
      _this->symbol_table_num = _this->symbol_table_size / sizeof(Elf32_Sym);
      /*  sh_size
        This  member  holds  the section's size in bytes.
        Unless the section type is SHT_NOBITS, the section occupies sh_size
        bytes in the file. A section of type SHT_NOBITS may have a nonzero
        size, but it occupies no space in the file.
       */
      find_symbol_table = true;
      _this->is_elf32 =
          _this->is_elf32 && !(_this->symbol_table_size % sizeof(Elf32_Sym));

      Assert(!_this->symbol_table_addr, "symbol table addr must be zero");
      Assert(_this->is_elf32, "symbol table size invalid");
      Log("find symbol table[%2d] addr: %08x  offset: %08x  size: %08x "
          "entries: %2d",
          i, _this->symbol_table_addr, _this->symbol_table_offset,
          _this->symbol_table_size, _this->symbol_table_num);
      break;

    case SHT_STRTAB:
      /* This section holds a string table.
       * An object file may have multiple string table sections.
       */
      // TODO: only consider single string table sections
      // there is also a dynamic stack table, however in nemu
      // we need not consider such thing
      _this->string_table_addr = _this->elf_section_header_table[i].sh_addr;
      _this->string_table_offset = _this->elf_section_header_table[i].sh_offset;
      _this->string_table_size = _this->elf_section_header_table[i].sh_size;

      Assert(!_this->string_table_addr, "string table addr must be zero");

      find_string_table = true;

      Log("find string table[%2d] addr: %08x  offset: %08x  size: %08x", i,
          _this->string_table_addr, _this->string_table_offset,
          _this->string_table_size);
      break;

    default:
      continue;
    }

    if (find_string_table && find_symbol_table)
      break;
  }

  if (!find_symbol_table) { // no symbol table
    Log("Not find symbol table");
    assert(0);
  } else if (!find_string_table) {
    Log("Not find string table");
    assert(0);
  }
}

static void read_symbol_table(Elf32_parser *const _this) {
  _this->elf_symbol_table = (Elf32_Sym *)malloc(_this->symbol_table_size);
  check_malloc(_this->elf_symbol_table);
  Log("malloc symbol_table size: %u each: %u", _this->symbol_table_size,
      (uint32_t)sizeof(Elf32_Sym));

  fseek(fp, _this->symbol_table_offset, SEEK_SET);
  fread(_this->elf_symbol_table, sizeof(Elf32_Sym), _this->symbol_table_num,
        fp);
}

static void read_string_table(Elf32_parser *const _this) {

  _this->elf_string_table =
      (char *)malloc(_this->string_table_size * sizeof(char));
  check_malloc(_this->elf_string_table);
  fseek(fp, _this->string_table_offset, SEEK_SET);
  fread(_this->elf_string_table, sizeof(char), _this->string_table_size, fp);
}

static void parser_destructor(Elf32_parser *const _this) {
  free(_this->elf_section_header_table);
  free(_this->elf_symbol_table);
  free(_this->elf_string_table);
}

static void parser_constructor(Elf32_parser *const _this) {
  _this->is_elf32 = true;
}

static int elf_parser() {

  static Elf32_parser this_parser; // I don't want to malloc here
  parser_constructor(&this_parser);

  /*parse elf header first*/
  read_elf_header(&this_parser);

  // TODO: need more check to guarantee that it is ELF32 file

  /*read section header table*/
  read_section_header_table(&this_parser);

  /*locate where the symbol table and string table*/
  locate_symbol_and_string_table(&this_parser);

  /* read symbol table*/
  read_symbol_table(&this_parser);

  /* read string table*/
  read_string_table(&this_parser);

  /*log all functions*/
  log_funcs(&this_parser);

  /* free malloc*/
  parser_destructor(&this_parser);

  fclose(fp);
  return 0;
}

int init_elf() {
  Log("FTrace %s", ANSI_FMT("ON", ANSI_FG_GREEN));

  if (elf_file == NULL) {
    Log("No elf file given");
    return -1;
  }

  Assert(elf_file, "FILE cannot be empty");
  fp = fopen(elf_file, "rb"); // read binary elf file
  Assert(fp, "Can not open ELF file %s", elf_file);
  Log("%s file [%s]", ANSI_FMT("LOAD ELF", ANSI_FG_GREEN), elf_file);

  return elf_parser();
}

#endif
