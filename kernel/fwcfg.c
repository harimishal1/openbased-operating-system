#include <error.h>
#include <assert.h>
#include <string.h>
#include <x86-64/asm.h>
#include <kernel/fwcfg.h>


struct fwcfg_file fwcfg_files[FW_CFG_MAX_FILES];
uint32_t fwcfg_file_count = 0;

int has_fwcfg() {
	uint8_t result[4];
	
	// Detect the presence of FW_CFG
	outw(FW_CFG_PORT_SEL, FW_CFG_SIGNATURE);
	for(int i = 0; i < 4; i++) {
		result[i] = inb(FW_CFG_PORT_DATA);
	}

	return strncmp((char *) result, "QEMU", 4) == 0;
}

int fwcfg_init() {
	assert(has_fwcfg());

	uint32_t count = 0;

	// Read the number of files present, big-endian
	outw(FW_CFG_PORT_SEL, FW_CFG_FILE_DIR);
	for(int i = 0; i < 4; i++) {
		count |= inb(FW_CFG_PORT_DATA) << (8 * (3 - i));
	}

	assert(count < FW_CFG_MAX_FILES);
	fwcfg_file_count = count;

	// Read all the file metadata present
	for(int i = 0; i < count; i++) {
		// Read the size field in big-endian
		for(int j = 0; j < 4; j++) {
			fwcfg_files[i].size |= inb(FW_CFG_PORT_DATA) << (8 * (3 - j));
		}

		// Read the selector key in big-endian
		fwcfg_files[i].selector |= inb(FW_CFG_PORT_DATA) << 8;
		fwcfg_files[i].selector |= inb(FW_CFG_PORT_DATA);

		// Skip two bytes
		inb(FW_CFG_PORT_DATA);
		inb(FW_CFG_PORT_DATA);

		// Read remaining 56 bytes for the filename
		for(int j = 0; j < 56; j++) {
			fwcfg_files[i].name[j] = inb(FW_CFG_PORT_DATA);
		}
	}

	return count;
}

int fwcfg_find_file(const char *name) {
	int file_index;
	for(file_index = 0; file_index < fwcfg_file_count; file_index++) {
		if(strncmp(name, fwcfg_files[file_index].name, 56) == 0)
			break;
	}

	if(file_index == fwcfg_file_count)
		return -EINVAL;

	return file_index;
}

int fwcfg_read_file(struct fwcfg_file file, char *buf) {
	outw(FW_CFG_PORT_SEL, file.selector);
	for(int i = 0; i < file.size; i++) {
		buf[i] = inb(FW_CFG_PORT_DATA);
	}

	return file.size;
}

int fwcfg_read(const char *name, char *buf, int n) {
	// First, we need to find the file in our results
	int file_index = fwcfg_find_file(name);
	if(file_index < 0)
		return file_index;

	struct fwcfg_file file = fwcfg_files[file_index];
	if(file.size > n)
		return -ENOMEM;

	return fwcfg_read_file(file, buf);
}

