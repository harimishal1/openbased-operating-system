/* Map over a big page a smaller one (partial overlaps, split). */
#include <lib.h>

int main(int argc, char **argv)
{
     char *addr = (void *)0x1000000;
     char *ret;

     ret = mmap(addr, 3 * PAGE_SIZE, PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret == addr);


     /* Try to map over part of the existing region with MAP_FIXED */
     ret = mmap(addr + PAGE_SIZE, PAGE_SIZE, PROT_READ,
                MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
     assert(ret != MAP_FAILED);

     print_vmas();

     return 0;
}
