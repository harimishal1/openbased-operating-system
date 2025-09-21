/* Map over 3 different pages one big page (full overlap, merge). */
#include <lib.h>

int main(int argc, char **argv)
{
     char *addr = (void *)0x1000000;
     char *ret;

     ret = mmap(addr, PAGE_SIZE, PROT_READ,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret == addr);

     ret = mmap(addr + PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret == addr + PAGE_SIZE);

     ret = mmap(addr + 2 * PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_EXEC,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret == addr + 2 * PAGE_SIZE);

     /* Now map over all of them with a single call */
     ret = mmap(addr, 3 * PAGE_SIZE, PROT_READ | PROT_EXEC,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret == addr);

     print_vmas();

     return 0;
}
