#include "x86-64/types.h"
#include <string.h>
#include <types.h>
#include <kernel/dev/disk.h>
#include <list.h>
#include <lib.h>
#include <kernel/mem/swap.h>
#include<kernel/mem/buddy.h>
#include<kernel/mem/lookup.h>

#define SECTOR_SIZE 512
#define SWAP_SECTORS_PER_PAGE (PAGE_SIZE / SECTOR_SIZE)
#define SWAP_SIZE_MB 128
#define SWAP_ENTRIES ((SWAP_SIZE_MB * 1024 * 1024) / PAGE_SIZE) 

static unsigned long swap_bitmap[SWAP_ENTRIES / (sizeof(unsigned long) * 8)];

struct swap_entry {
    size_t sector;      
    struct task *task;  
    uint64_t pte;
    bool used;
};

static struct swap_entry swap_meta[SWAP_ENTRIES];

struct disk *sdisk = NULL;

static struct swap_entry swap_entries[SWAP_ENTRIES];

int swap_init(void)
{
    extern struct disk *disks[];
    extern size_t ndisks;

    if (ndisks < 2) {
        cprintf("[swap] no secondary disk available\n");
        return -1;
    }

    sdisk = disks[1]; 

    cprintf("[swap] using disk %p for swap\n", sdisk);
    
    struct page_info *swap_buffer = page_alloc(ALLOC_ZERO);
    swap_buffer->pp_ref++;

    cprintf("[swap] swap initialized successfully\n");
    memset(swap_bitmap, 0, sizeof(swap_bitmap));
    memset(swap_meta, 0, sizeof(swap_meta));

    cprintf("[swap] initialized %d slots (%.2f MB swap)\n",
            SWAP_ENTRIES, (SWAP_ENTRIES * PAGE_SIZE) / (1024.0 * 1024.0));
    
    return 0;
}

int swap_page_in(struct page_info *pp, size_t slot)
{

    uint64_t lba = slot * SWAP_SECTORS_PER_PAGE;
    void *buf = page2kva(pp);

    int64_t ret;
    do {
        ret = disk_read(sdisk, buf, SWAP_SECTORS_PER_PAGE, lba);
        if (ret == -EAGAIN) {
            disk_poll(sdisk);  // wait until ready
        }
    } while (ret == -EAGAIN);

    if (ret < 0) {
        cprintf("[swap] page_in failed: disk_read error %ld (slot %zu)\n",
                ret, slot);
        return ret;
    }

    return 0;
}

int swap_page_out(struct page_info *pp, size_t slot)
{
    if (!sdisk) {
        cprintf("[swap] page_out failed: no swap disk\n");
        return -1;
    }

    if (!pp || !pp->owner) {
        cprintf("[swap] page_out failed: invalid page\n");
        return -EINVAL;
    }

    struct task *task = pp->owner;
    uint64_t va = pp->virt_addr;
    uint64_t lba = slot * SWAP_SECTORS_PER_PAGE;
    void *buf = page2kva(pp);

    int64_t ret;
    do {
        ret = disk_write(sdisk, buf, SWAP_SECTORS_PER_PAGE, lba);
        if (ret == -EAGAIN)
            disk_poll(sdisk);
    } while (ret == -EAGAIN);

    if (ret < 0) {
        cprintf("[swap] disk write failed at slot %zu (va=%p pid=%d)\n",
                slot, (void *)va, task->task_pid);
        return (int)ret;
    }

    swap_entries[slot].sector = lba;
    swap_entries[slot].task = task;
    swap_entries[slot].pte = va;
    swap_entries[slot].used = true;

    physaddr_t *pte = (physaddr_t*)page_lookup(task->task_pml4, (void *)va, 0);
    if (pte && (*pte & PAGE_PRESENT)) {
        *pte &= ~PAGE_PRESENT;
    }

    // Free physical frame
    page_free(pp);

    cprintf("[swap] swapped out va=%p pid=%d slot=%zu\n",
            (void *)va, task->task_pid, slot);

    return 0;
}

   