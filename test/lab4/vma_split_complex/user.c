/* Maps a region, then mprotect skipping 1 PAGE_SIZE changes the permissions. */
#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *addr = (void *)0x1000000;

    for (int i = 0; i < 4; i++)
    {
        char *new_addr = addr + i * PAGE_SIZE;

        char *ret = mmap(new_addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
        assert(ret != MAP_FAILED);
    }
   
    for (int i = 0; i < 2; i++)
    {
        char *new_addr = addr + i * PAGE_SIZE * 2;
        int ret = mprotect(new_addr, PAGE_SIZE, PROT_READ | PROT_EXEC);
        assert(ret == 0);
    }

    print_vmas();

    return 0;
}
