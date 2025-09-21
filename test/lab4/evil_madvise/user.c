#include <lib.h>

int main(int argc, char **argv)
{
	char *addr = (void *)0xffff800000000000ull;

	// madvise call should fail
	assert(madvise(addr, 0xffffffffffffffffull - 0xffff800000000000ull,
	    MADV_DONTNEED) != 0);

	printf("This should not have crashed!\n");
	print_vmas();

	return 0;
}
