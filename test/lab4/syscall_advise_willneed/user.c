#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct vma_info info;
	char *addr = (void *)0x1000000;

	mmap(addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
	     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	madvise(addr, PAGE_SIZE, MADV_WILLNEED);
	mquery(&info, addr);
	memset(addr, 0, PAGE_SIZE);

	assert(info.vm_mapped == VM_4K_PAGE);

	print_vmas();

	return 0;
}
