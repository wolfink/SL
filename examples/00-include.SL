## def
#include <stdio.h>
#include <stdlib.h>
int replace(FILE* out, int argc, char** argv) {
        if (argc < 2) return EXIT_FAILURE;
        fprintf(out, "\t.globl\t%s\n", argv[1]);
        fprintf(out, "\t.type\t%s,@function\n", argv[1]);
        fprintf(out, "%s:\n", argv[1]);
        return EXIT_SUCCESS;
}
##
## echo
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int print_index=0;
int replace(FILE* out, int argc, char** argv) {
        for (int i = 1; i < argc; i++, print_index++) {
                fprintf(out, "\t.data\n");
                fprintf(out, ".print%d:\n", print_index);
                if (i < argc - 1) {
                        fprintf(out, "\t.ascii \"%s \"\n", argv[i]);
                } else {
                        fprintf(out, "\t.ascii \"%s\\n\"\n", argv[i]);
                }
                fprintf(out, "\t.text\n");
                fprintf(out, "\tmovq\t$1, %%rax\n");
                fprintf(out, "\tmovq\t$1, %%rdi\n");
                fprintf(out, "\tleaq\t.print%d(%%rip), %%rsi\n", print_index);
                fprintf(out, "\tmovq\t$%d, %%rdx\n", strlen(argv[i]) + 1);
                fprintf(out, "\tsyscall\n");
        }
        return EXIT_SUCCESS;
}
##
## exit
#include <stdio.h>
#include <stdlib.h>
int replace(FILE* out, int argc, char** argv) {
        fprintf(out, "\tmovq\t$60, %%rax\n");
        fprintf(out, "\tsyscall\n");
        return EXIT_SUCCESS;
}
##
