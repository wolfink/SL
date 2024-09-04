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
#define MAX_ARGS 256
#define MAX_MACRO_NAME 64
#define MAX_MACROS 1024

struct stat st={0};

int macro_list_len=0;

typedef struct string {
        int len;
        char data[MAX_LINE];
} sl_string;

typedef struct macro {
        sl_string name;
        void (*replace)(FILE*, int argc, char** argv);
} macro;
macro macro_list[MAX_MACROS];

int send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
char* send_argv[MAX_ARGS];

int main(int argc, char** argv)
#define FAIL return EXIT_FAILURE;
{
        assert(argc > 1)
        FILE* in=fopen(argv[1], "r");
        // Create /tmp/sl directory if none exists
        if (stat("/tmp/sl", &st) == -1) {
                mkdir("/tmp/sl", 0700);
        }
        FILE* out=fopen("/tmp/sl/out.s", "w");
        assert(in != NULL)
        assert(out != NULL)
        char line[MAX_LINE];
        int cpy_into_temp=0;

        sl_string temp_name;
        FILE* temp;
        // Find a macro, copy its code into a c file
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                // Match lines starting with ## as macro definitions
                int i=0; if (line[i] == '#' && line[++i] == '#') {
                        if (cpy_into_temp) {
                                macro* mp=&macro_list[macro_list_len-1];
                                sl_string tempso_name;
                                tempso_name.len=sprintf(tempso_name.data, "/tmp/sl/%s.so", mp->name.data);

                                assert(fclose(temp) == EXIT_SUCCESS)
                                pid_t p=fork();
                                if (p == 0) {
                                        execvp("gcc", (char*[MAX_LINE]){"gcc", "-g", "-fPIC", "-shared", temp_name.data, "-o", tempso_name.data});
                                }
                                wait(NULL);
                                void* shared_object_handle=dlopen(tempso_name.data, RTLD_NOW);
                                assert(shared_object_handle != NULL);
                                mp->replace=dlsym(shared_object_handle, "replace");
                                assert(mp->replace != NULL);
                                cpy_into_temp=0;
                                continue;
                        } else {
                                // Create macro
                                macro* mp=&macro_list[macro_list_len++];
                                // Copy macro name
                                while (iswspace(line[++i]));
                                int j=0;
                                while (isalnum(line[i]) || line[i] == '_') {
                                        mp->name.data[j++]=line[i++];
                                }
                                mp->name.len=j;
                                // Open macro temp file
                                temp_name.len=sprintf(temp_name.data, "/tmp/sl/%s.c", mp->name.data);
                                temp=fopen(temp_name.data, "w");
                                assert(temp != NULL)
                                cpy_into_temp=1;
                        }
                } else if (cpy_into_temp) {
                        fputs(line, temp);
                } else for (int i=0; i < MAX_LINE && line[i] != '\0'; i++) {
                        // If there is a macro, check if line matches macro
                        if (macro_list_len > 0) for (int mli=0; mli < macro_list_len; mli++) {
                                macro* mp=&macro_list[mli];
                                if (i + mp->name.len < MAX_LINE && strncmp(mp->name.data, line+i, mp->name.len) == 0) {
                                        assert(strncpy(send_argv_buffer[0], mp->name.data, MAX_LINE) == send_argv_buffer[0]);
                                        i += mp->name.len;
                                        send_argc=1;
                                        send_argv[0]=send_argv_buffer[0];
                                        while (line[i] != '\n') {
                                                while(iswspace(line[i])) i++;
                                                int k=0;
                                                for(; !iswspace(line[i]); i++, k++) {
                                                        send_argv_buffer[send_argc][k]=line[i];
                                                }
                                                send_argv_buffer[send_argc][k]='\0';
                                                send_argv[send_argc]=send_argv_buffer[send_argc];
                                                send_argc++;
                                        }
                                        mp->replace(out, send_argc, send_argv);
                                }
                        }
                        putc(line[i], out);
                }

        }
        assert(fclose(out) == EXIT_SUCCESS)
        pid_t p=fork();
        if (p == 0) {
                execvp("as", (char*[8]){"as", "/tmp/sl/out.s", "-o", "/tmp/sl/out.o"});
        }
        wait(NULL);
        execvp("ld", (char*[8]){"ld", "/tmp/sl/out.o", "-o", "a.out"});
}
#undef FAIL
