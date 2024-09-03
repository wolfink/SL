#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h>

#define eprintf(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define sal_error_msg(fmt,...) eprintf(__FILE__ ":%d: " fmt, __LINE__, ##__VA_ARGS__)
#define assert(expr,...) if(!(expr)) { sal_error_msg("failed assertion: %s\n", #expr); FAIL }
#define MAX_LINE 256

struct stat st={0};

int main(int argc, char** argv)
#define FAIL return EXIT_FAILURE;
{
        assert(argc > 1)
        FILE* in=fopen(argv[1], "r");
        if (stat("/tmp/sl", &st) == -1) {
                mkdir("/tmp/sl", 0700);
        }
        FILE* temp=fopen("/tmp/sl/temp.c", "w");
        FILE* out=fopen("/tmp/sl/out.s", "w");
        assert(in != NULL)
        assert(temp != NULL)
        assert(out != NULL)
        char line[MAX_LINE];
        char macro_name[MAX_LINE];
        int macro_name_length=-1;
        int cpy_into_temp=0;
        // Find a macro, copy its code into temp.c
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                // Match lines starting with ## as macro definitions
                int i=0; if (line[i] == '#' && line[++i] == '#') {
                        if (cpy_into_temp) {
                                cpy_into_temp=0;
                                break;
                        }
                        macro_name_length=0;
                        while (iswspace(line[++i]));
                        while (isalnum(line[i]) || line[i] == '_') {
                                macro_name[macro_name_length++]=line[i++];
                        }
                        cpy_into_temp=1;
                } else if (cpy_into_temp) fputs(line, temp);
        }
        assert(macro_name_length > 0)
        assert(fclose(temp) == EXIT_SUCCESS)
        pid_t p=fork();
        if (p == 0) {
                execvp("gcc", (char*[8]){"gcc", "-shared", "/tmp/sl/temp.c", "-o", "/tmp/sl/temp.so"});
        }
        wait(NULL);
        void* handle=dlopen("/tmp/sl/temp.so", RTLD_NOW);
        assert(handle != NULL);
        int(*replace)(FILE*)=dlsym(handle, "replace");
        assert(replace != NULL);
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                for (int i=0; i < MAX_LINE && line[i] != '\0'; i++) {
                        if (macro_name_length > 0
                                && i + macro_name_length < MAX_LINE
                                && strncmp(macro_name, line + i, macro_name_length) == 0) {
                                replace(out);
                                i += macro_name_length;
                        }
                        putc(line[i], out);
                }
        }
        assert(fclose(out) == EXIT_SUCCESS);
        p=fork();
        if (p == 0) {
                execvp("as", (char*[8]){"as", "/tmp/sl/out.s", "-o", "/tmp/sl/out.o"});
        }
        wait(NULL);
        execvp("ld", (char*[8]){"ld", "/tmp/sl/out.o", "-o", "a.out"});
}
#undef FAIL
