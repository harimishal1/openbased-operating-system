/* Different permission, should not merge */
#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *addr = (void *)0x1000000;
    
    /* Create adjacent mappings with different permissions */
    assert(mmap(addr, PAGE_SIZE, PROT_READ,
         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == addr);
    
    assert(mmap(addr + PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE,
         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0) == addr + PAGE_SIZE);
    
    print_vmas();
    
    /* Now make them the same and they should merge */
    assert(mprotect(addr + PAGE_SIZE, PAGE_SIZE, PROT_READ) == 0);
    
    print_vmas();
    
    return 0;
}
