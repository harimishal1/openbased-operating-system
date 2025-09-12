#include <lib.h>

int zero;

/* Trigger a division by zero exception. */
int main(int argc, char **argv)
{
	/* DO 1/0 in inline assembly as clang will optimize
	    out the previous approach at O1 */
	asm("mov    $0x1, %rax\n\t"
	    "mov    $0x0, %rcx\n\t"
	    "cltd\n\t"
	    "idiv %rcx");
}
