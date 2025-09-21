/* Creates multiple VMAs and changes permissions, should be merged. */
#include <lib.h>

int main(int argc, char **argv)
{
     char *base_addr = (void *)0x1000000;

     /* Create a pattern of regions with different permissions
      * RRRR-RWRW-RRRR
      */

     mmap(base_addr, 4 * PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

     /* Two alternating pages (read-write, read-only) */
     mmap(base_addr + 5 * PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
     mmap(base_addr + 6 * PAGE_SIZE, PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
     mmap(base_addr + 7 * PAGE_SIZE, PAGE_SIZE, PROT_READ | PROT_WRITE,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
     mmap(base_addr + 8 * PAGE_SIZE, PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

     /* Final region (read-only) */
     mmap(base_addr + 10 * PAGE_SIZE, 4 * PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

     /* printf("Initial VMA layout:\n");
      * print_vmas();
      */

     /* Now fill in the gaps with same permissions to test merging */
     mmap(base_addr + 4 * PAGE_SIZE, PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

     /* printf("After filling first gap (should merge with first region):\n");
      * print_vmas();
      */

     mmap(base_addr + 9 * PAGE_SIZE, PAGE_SIZE, PROT_READ,
          MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

     /* printf("After filling second gap (should merge with final region):\n");
      * print_vmas();
      */

     /* Convert the read-write pages to read-only and check if everything merges */
     mprotect(base_addr + 5 * PAGE_SIZE, PAGE_SIZE, PROT_READ);
     mprotect(base_addr + 7 * PAGE_SIZE, PAGE_SIZE, PROT_READ);

     /* After changing all regions to read-only (should all merge). */
     print_vmas();

     return 0;
}
