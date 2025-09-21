#include <lib.h>

int main(int argc, char **argv)
{
	struct vma_info info;
	const char *s = "Hello, world!";
	void *addr = NULL;

	while (addr < (void *)USER_LIM) {
		if (mquery(&info, addr) < 0) {
			break;
		}

		addr = info.vm_end;

		if (info.vm_type == VMA_FREE) {
			continue;
		}
		
		if (strcmp(info.vm_name, ".data") != 0) {
			continue;
		}

		assert(info.vm_mapped == VM_UNMAPPED);
	}

	print_vmas();

	return 0;
}
