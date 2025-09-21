/* Try to munmap an unaligned address. */
#include <lib.h>

int main(int argc, char **argv)
{
    char *addr = (void *)0x1000000;
    char *ret;
    
    ret = mmap(addr, 4 * PAGE_SIZE, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(ret == addr);

    print_vmas();

    /* unmap with unaligned addr and size */
    void *unaligned_addr = (void *)(addr + 100); // Not page-aligned
    size_t unaligned_size = 2 * PAGE_SIZE + 200; // Not page-size aligned

    /* Should align to page boundaries */
    munmap(unaligned_addr, unaligned_size);

    print_vmas();

    return 0;
}
