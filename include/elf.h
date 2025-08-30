#pragma once

#define ELF_MAGIC 0x464C457FU   /* "\x7FELF" in little endian */

struct elf {
	uint32_t e_magic;   /* must equal ELF_MAGIC */
	uint8_t  e_elf[12];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

struct elf_proghdr {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_va;
	uint64_t p_pa;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

struct elf_secthdr {
	uint32_t sh_name;
	uint32_t sh_type;
	uint64_t sh_flags;
	uint64_t sh_addr;
	uint64_t sh_offset;
	uint64_t sh_size;
	uint32_t sh_link;
	uint32_t sh_info;
	uint64_t sh_addralign;
	uint64_t sh_entsize;
};

struct elf_dyn {
	uint64_t d_tag;
	uint64_t d_val;
};

struct elf_sym {
	uint32_t s_name;
	uint8_t  s_info;
	uint8_t  s_other;
	uint16_t s_shndx;
	uint64_t s_value;
	uint64_t s_size;
};

/* Values for elf_prog_hdr::p_type */
#define ELF_PROG_LOAD 1

/* Flag bits for elf_prog_hdr::p_flags */
#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4

/* Values for elf_sect_hdr::sh_type */
#define ELF_SHT_NULL     0
#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB   2
#define ELF_SHT_STRTAB   3

/* Values for elf_sect_hdr::sh_name */
#define ELF_SHN_UNDEF 0

/* Values for elf_dynentry::d_tag */
#define ELF_DYN_NULL      0
#define ELF_DYN_STRTAB    5
#define ELF_DYN_SYMTAB    6
#define ELF_DYN_STRSIZE 0xA
#define ELF_DYN_SYMSIZE 0xB

/* Flags for elf_sym::st_info */
#define ELF_SYM_BIND(info) ((info) >> 4)
#define ELF_SYM_TYPE(info) ((info) & 0xF)

/* Values for elf_sym::st_info >> BIND */
#define ELF_SYM_BIND_LOCAL  0
#define ELF_SYM_BIND_GLOBAL 1

/* Values for elf_sym::st_info >> TYPE */
#define ELF_SYM_TYPE_NOTYPE  0
#define ELF_SYM_TYPE_OBJECT  1
#define ELF_SYM_TYPE_FUNC    2
#define ELF_SYM_TYPE_SECTION 3
#define ELF_SYM_TYPE_FILE    4

/* Flags for elf_sym::st_other */
#define ELF_SYM_VISIBILITY(other) ((other) & 0x3)
