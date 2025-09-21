/* Test mmap with NULL (should work if MAP_FIXED is not set). */
#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
    void *addr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE,
         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    assert(addr != MAP_FAILED);
    
    /* Confirming correct functioning of mapping with an operation. */
    strcpy(addr, "test data");
    assert(strcmp(addr, "test data") == 0);
    
    print_vmas();
    
    return 0;
}
