#pragma once

#include <types.h>

// I/O port locations for communication with QEMU fw_cfg
#define FW_CFG_PORT_SEL     0x510
#define FW_CFG_PORT_DATA    0x511
#define FW_CFG_PORT_DMA     0x514

// Selectors for fw_cfg
#define FW_CFG_SIGNATURE    0x0000
#define FW_CFG_ID           0x0001
#define FW_CFG_FILE_DIR     0x0019

#define FW_CFG_MAX_FILES    64

/**
 * A single fw_cfg file
 * 
 * @param size The size of the file
 * @param selector The selector key to be passed to FW_CFG_PORT_SEL
 * @param name The name of the file
 */
struct fwcfg_file {
    uint32_t size;
    uint16_t selector;
    uint16_t _;
    char name[56];
};

// Static buffer of files
extern struct fwcfg_file fwcfg_files[FW_CFG_MAX_FILES];
extern uint32_t fwcfg_file_count;

int has_fwcfg();
int fwcfg_init();
int fwcfg_read(const char *name, char *buf, int n);
