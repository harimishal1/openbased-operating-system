#include <lib.h>

int main(int argc, char **argv)
{
	struct vma_info info;
	char *addr = (void *)0x1000000;

	mmap(addr, PAGE_SIZE, PROT_READ | PROT_WRITE,
	     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	
	// Access the data to ensure it is loaded
	*(volatile char *)addr;
	mquery(&info, addr);
	assert(info.vm_mapped != VM_UNMAPPED);

	// Advise don't need
	madvise(addr, PAGE_SIZE, MADV_DONTNEED);
	mquery(&info, addr);
	assert(info.vm_mapped == VM_UNMAPPED);

	print_vmas();

	return 0;
}
