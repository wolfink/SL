#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <pthread.h>

#define eprintf(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define sal_error_msg(fmt,...) eprintf(__FILE__ ":%d: " fmt, __LINE__, ##__VA_ARGS__)
#define assert(expr,...) if(!(expr)) { sal_error_msg("failed assertion: %s\n", #expr); __VA_ARGS__ FAIL }
#define MAX_LINE 256
#define MAX_ARGS 256
#define MAX_MACRO_NAME 64
#define MAX_MACROS 1024

struct stat st={0};

int macro_list_len=0;

typedef struct string {
        char data[MAX_LINE];
        int len;
} sl_string;

typedef struct macro {
        sl_string name;
        void (*replace)(FILE*, int argc, char** argv);
} macro;
macro macro_list[MAX_MACROS];

int send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
char* send_argv[MAX_ARGS];

char* tmp_dir=NULL;

/* Generate assembly from SL code. */
void* sl_generate(void*);
/* Loads a macro function call into send_argc and send_argv. */
int sl_load_macro_args(macro* mp, char* line, int start);
/* Processes a line of input. */
int sl_process_line (FILE* out, char* line);
/* Create a folder with subfolders. */
int sl_install(char* file, __mode_t mode);
/* Reads a macro definition. */
int sl_read_macro(FILE* src, char* line);
/* Finds next non-whitespace index. */
inline int sl_skip_whitespace(char* line, int start);

void* sl_generate(void* vargp)
#define FAIL return NULL;
{
        assert(vargp != NULL)
        char* filename=vargp;
        FILE* in=fopen(filename, "r");
        sl_string tmp_filedir;
        char* filename_no_ext=strtok(filename, ".");
        assert(filename_no_ext != NULL)
        tmp_filedir.len=sprintf((char*) &tmp_filedir, "/tmp/sl/%s", filename_no_ext);
        // Install tmp directories
        assert(sl_install((char*) &tmp_filedir, 0700) == EXIT_SUCCESS);
        sl_string outfile_name;
        outfile_name.len=sprintf((char*) &outfile_name, "%s/out.s", tmp_filedir.data);
        FILE* out=fopen((char*) &outfile_name, "w");
        assert(in != NULL)
        assert(out != NULL)

        char line[MAX_LINE];
        // Find a macro, copy its code into a c file
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                tmp_dir=tmp_filedir.data;
                // Match lines starting with ## as macro definitions
                if (line[0] == '#' && line[1] == '#') { assert(sl_read_macro(in, line) == EXIT_SUCCESS); }
                // Match lines that start with "load" as macro loading
                else if (strncmp(line, "load", 4) == 0) {
                        int i=sl_skip_whitespace(line, 4);
                        assert(line[i] == '\"');
                        char* load_filename=strtok(line + i + 1, "\"");
                        sl_string load_file;
                        sl_string dir;
                        strcpy(dir.data, filename);
                        dir.len=strlen(dir.data);
                        int last_slash=0;
                        for (int j=0; j < dir.len && dir.data[j] != '/'; j++, last_slash++);
                        dir.data[last_slash]='\0';
                        load_file.len=sprintf((char*) &load_file, "%s/%s", dir.data, load_filename);
                        assert(load_file.data != NULL)
                        pthread_t thread_id;
                        assert(pthread_create(&thread_id, NULL, sl_generate, &load_file) == EXIT_SUCCESS);
                        assert(pthread_join(thread_id, NULL) == EXIT_SUCCESS);
                } else assert(sl_process_line(out, line) == EXIT_SUCCESS);
        }
        assert(fclose(out) == EXIT_SUCCESS)
        return vargp;
}
#undef FAIL

int sl_install(char* directory, __mode_t mode)
#define FAIL return EXIT_FAILURE;
{
        for (int i=0; directory[i] != '\0'; i++) {
                if (directory[i] == '/' && i > 0) {
                        directory[i] = '\0';
                        if (stat(directory, &st) == -1) assert(mkdir(directory, mode) == EXIT_SUCCESS,
                                printf("directory=%s, i=%d\n", directory, i);)
                        directory[i]='/';
                }
        }
        if (stat(directory, &st) == -1) assert(mkdir(directory, mode) == EXIT_SUCCESS,
                printf("directory=%s\n", directory);)
        return EXIT_SUCCESS;
}
#undef FAIL

int sl_load_macro_args(macro* mp, char* line, int start)
#define FAIL return -1;
{
        int i=start;
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
        return i;
}
#undef FAIL

int sl_process_line (FILE* out, char* line) {
#define FAIL return EXIT_FAILURE;
        // Match lines starting with ## as macro definitions
        for (int i=0; i < MAX_LINE && line[i] != '\0'; i++) {
                // If there is a macro, check if line matches macro
                if (macro_list_len > 0) for (int mli=0; mli < macro_list_len; mli++) {
                        macro* mp=&macro_list[mli];
                        if (i + mp->name.len < MAX_LINE && strncmp(mp->name.data, line+i, mp->name.len) == 0) {
                                i=sl_load_macro_args(mp, line, i);
                                assert(i >= 0);
                                mp->replace(out, send_argc, send_argv);
                        }
                }
                putc(line[i], out);
        }
        return EXIT_SUCCESS;
}
#undef FAIL

int sl_read_macro(FILE* src, char* line)
#define FAIL return EXIT_FAILURE;
{
        assert(tmp_dir != NULL)
        sl_string temp_name;
        FILE* temp;
        // Create macro
        macro* mp=&macro_list[macro_list_len++];
        // Copy macro name (skip "##" at the beginning)
        int i=sl_skip_whitespace(line, 2);
        int j=0;
        while (isalnum(line[i]) || line[i] == '_') {
                mp->name.data[j++]=line[i++];
        }
        mp->name.len=j;
        // Open macro temp file
        temp_name.len=sprintf(temp_name.data, "%s/%s.c", tmp_dir, mp->name.data);
        temp=fopen(temp_name.data, "w");
        assert(temp != NULL)
        while (fgets(line, MAX_LINE-1, src) != NULL) {
                if (line[0] == '#' && line[1] == '#') {
                        macro* mp=&macro_list[macro_list_len-1];
                        sl_string tempso_name;
                        tempso_name.len=sprintf(tempso_name.data, "%s/%s.so", tmp_dir, mp->name.data);

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
                        return EXIT_SUCCESS;
                }
                fputs(line, temp);
        }
        eprintf("macro must end with \"##\"\n");
        return EXIT_FAILURE;
}
#undef FAIL

int sl_skip_whitespace(char* line, int start)
{
        int i=start - 1;
        while(iswspace(line[++i]));
        return i;
}

int main(int argc, char** argv)
#define FAIL return EXIT_FAILURE;
{
        assert(argc > 1)
        assert(sl_generate(argv[1]) != NULL)
        assert(tmp_dir != NULL)
        sl_string as_file;
        sl_string o_file;
        as_file.len=sprintf(as_file.data, "%s/out.s", tmp_dir);
        o_file.len=sprintf(o_file.data, "%s/out.o", tmp_dir);
        pid_t p=fork();
        if (p == 0) {
                execvp("as", (char*[MAX_LINE]){"as", as_file.data, "-o", o_file.data});
        }
        assert(wait(NULL) != -1);
        execvp("ld", (char*[MAX_LINE]){"ld", o_file.data, "-o", "a.out"});
}
#undef FAIL
