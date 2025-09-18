/* Marks a page as RW, then as RX and executes it. */
#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *addr = (void *)0x1000000;
    char *ret;
    int ret_p;

    /* RW mapping */
    ret = mmap(addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(ret == addr);
    
    /* Fill with ret instructions */
    memset(addr, 0xC3, PAGE_SIZE);
    
    print_vmas();
    
    /* Allow execution */
    ret_p = mprotect(addr, PAGE_SIZE, PROT_READ | PROT_EXEC);
    assert(ret_p == 0);
    
    print_vmas();
    
    /* Execute the page */
    ((void (*)(void))addr)();
    
    printf("Successfully executed code\n");
    
    return 0;
}
