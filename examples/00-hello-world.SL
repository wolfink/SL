## print_hello_world
#include <stdio.h>
#include <stdlib.h>
int replace(FILE* out) {
        fprintf(out, "movq\t$1, %%rax\n");
        fprintf(out, "\tmovq\t$1, %%rdi\n");
        fprintf(out, "\tleaq\thello_world(%rip), %rsi\n");
        fprintf(out, "\tmovq\t$14, %%rdx\n");
        fprintf(out, "\tsyscall\n");
        return EXIT_SUCCESS;
}
##
## exit
#include <stdio.h>
#include <stdlib.h>
int replace(FILE* out) {
        fprintf(out, "movq\t$60, %%rax\n");
        fprintf(out, "\txor\t%%rdi, %%rdi\n");
        fprintf(out, "\tsyscall\n");
        return EXIT_SUCCESS;
}
##
	.data
hello_world:
	.ascii "Hello, world!\n"
	.text
	.globl  _start
	.type	_start,@function
_start:
        print_hello_world
        print_hello_world
        print_hello_world
        exit

