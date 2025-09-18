#include <lib.h>
#include <string.h>

int main(int argc, char **argv)
{
	struct vma_info info;
	char *addr = (void *)0x1000000;

	mmap(addr, HPAGE_SIZE, PROT_READ | PROT_WRITE,
	     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	memset(addr, 0, HPAGE_SIZE);
	mquery(&info, addr);

	assert(info.vm_mapped == VM_2M_PAGE);

	print_vmas();

	return 0;
}
