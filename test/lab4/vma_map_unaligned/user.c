/* Try to map an unaligned address. */
#include <lib.h>

int main(int argc, char **argv)
{
    char *addr = (void *)0x1000000;
    char *ret;

    void *unaligned_addr = (void *)(addr + 100); // Not page-aligned
    size_t unaligned_size = PAGE_SIZE + 200; // Not page-size aligned
    
    ret = mmap(unaligned_addr, unaligned_size, PROT_READ | PROT_WRITE,
               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(ret == addr);

    print_vmas();

    return 0;
}
