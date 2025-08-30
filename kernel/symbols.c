#include "kernel/symbols.h"

#include <string.h>
#include <types.h>
#include <assert.h>
#include <elf.h>
#include <types.h>

// These addresses are defined by the linker script, referring to
// the start of the .dynamic section, and the end (+1) of the
// .dynsym section.
extern char sdynamic[];
extern char edynsym[];

static bool symbols_found = false;
static struct elf_sym *symbol_table = NULL;
static char *string_table = NULL;

static void find_symbol_table() {
	if(symbols_found) return;

	// Iterate through the entries in the .dynamic section to find the
	// location of the relevant symbol and string tables.
	struct elf_dyn *dynamic = (struct elf_dyn *) sdynamic;
	while(dynamic->d_tag != ELF_DYN_NULL) {
		if(dynamic->d_tag == ELF_DYN_SYMTAB)
			symbol_table = (struct elf_sym *) dynamic->d_val;

		if(dynamic->d_tag == ELF_DYN_STRTAB)
			string_table = (char *) dynamic->d_val;

		dynamic++;
	}

	if(symbol_table != NULL && string_table != NULL) {
		symbols_found = true;
	} else {
		panic("Could not find the necessary symbol and string tables!\n");
	}
}

void *find_symbol(char *name) {
	// Ensure we know where to find the symbol table
	find_symbol_table();

	struct elf_sym *symbol = symbol_table;
	while(symbol < (struct elf_sym *) edynsym) {
		char *symbol_name = string_table + symbol->s_name;

		if(strcmp(name, symbol_name) == 0)
			return (void *) symbol->s_value;

		symbol++;
	}

	return NULL;
}
