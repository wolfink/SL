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
#include <assert.h>
#include <errno.h>

#define eprintf(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define sl_error_msg(fmt,...) eprintf(__FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#define sl_handle_err(expr,...) if(expr) { __VA_ARGS__ FAIL }
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
        int (*prologue)();
        int (*replace)(FILE*, int argc, char** argv);
} macro;
macro macro_list[MAX_MACROS];

int send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
char* send_argv[MAX_ARGS];

char* tmp_dir=NULL;

/* Generate assembly from SL code. */
void* sl_generate(void*);
/* Loads a macro function call into send_argc and send_argv. */
int sl_load_macro_args(macro* mp, char* line);
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
        assert(vargp != NULL);

        char* filename=vargp;
        FILE* in=fopen(filename, "r");
        sl_handle_err(in == NULL,
                sl_error_msg("failed to open %s: %s", filename, strerror(errno));)
        sl_string tmp_filedir;
        char* filename_no_ext=strtok(filename, ".");
        sl_handle_err (filename_no_ext == NULL,
                sl_error_msg("filename has no body");)
        tmp_filedir.len=sprintf((char*) &tmp_filedir, "/tmp/sl/%s", filename_no_ext);
        // Install tmp directories
        sl_handle_err(sl_install((char*) &tmp_filedir, 0700) == EXIT_FAILURE,
                sl_error_msg("could not install %s", (char*) &tmp_filedir);)
        sl_string outfile_name;
        outfile_name.len=sprintf((char*) &outfile_name, "%s/out.s", tmp_filedir.data);
        FILE* out=fopen((char*) &outfile_name, "w");
        sl_handle_err(out == NULL,
                sl_error_msg("failed to open %s: %s", (char*) &outfile_name, strerror(errno));)

        char line[MAX_LINE];
        // Find a macro, copy its code into a c file
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                tmp_dir=tmp_filedir.data;
                // Match lines starting with ## as macro definitions
                if (line[0] == '#' && line[1] == '#') {
                        sl_handle_err(sl_read_macro(in, line),
                                sl_error_msg("failed to read macro");)
                }
                // Match lines that start with "load" as macro loading
                else if (strncmp(line, "load", 4) == 0) {
                        int i=sl_skip_whitespace(line, 4);
                        sl_handle_err(line[i] != '\"', sl_error_msg("expected \'\"\'");)
                        char* load_filename=strtok(line + i + 1, "\"");
                        sl_string load_file;
                        sl_string dir;
                        strcpy(dir.data, filename);
                        dir.len=strlen(dir.data);
                        int last_slash=0;
                        for (int j=0; j < dir.len && dir.data[j] != '/'; j++, last_slash++);
                        dir.data[last_slash]='\0';
                        load_file.len=sprintf((char*) &load_file, "%s/%s", (char*) &dir, load_filename);
                        pthread_t thread_id;
                        sl_handle_err(pthread_create(&thread_id, NULL, sl_generate, &load_file))
                        sl_handle_err(pthread_join(thread_id, NULL));
                } else assert(sl_process_line(out, line) == EXIT_SUCCESS);
        }
        sl_handle_err(fclose(out), sl_error_msg("fclose failed: %s", strerror(errno));)
        return vargp;
}
#undef FAIL

int sl_install(char* directory, __mode_t mode)
#define FAIL return EXIT_FAILURE;
{
        assert(directory != NULL);

        for (int i=0; directory[i] != '\0'; i++) {
                if (directory[i] == '/' && i > 0) {
                        directory[i] = '\0';
                        if (stat(directory, &st) == -1) {
                                sl_handle_err(mkdir(directory, mode),
                                        sl_error_msg("failed to create directory %s: %s\n", directory, strerror(errno));)
                        }
                        directory[i]='/';
                }
        }
        if (stat(directory, &st) == -1) {
                sl_handle_err(mkdir(directory, mode),
                        sl_error_msg("failed to create directory %s: %s\n", directory, strerror(errno));)
        }
        return EXIT_SUCCESS;
}
#undef FAIL

int sl_load_macro_args(macro* mp, char* line)
#define FAIL return EXIT_FAILURE;
{
        assert(mp != NULL);
        assert(line != NULL);

        send_argc=0;
        char* arg=line;
        do send_argv[send_argc++]=arg;
        while ((arg=strtok(NULL, " \n")) != NULL);
        return EXIT_SUCCESS;
}
#undef FAIL

int sl_process_line (FILE* out, char* line)
#define FAIL return EXIT_FAILURE;
{
        assert(out != NULL);
        assert(line != NULL);

        char* word=strtok(line, " \n");
        if (word == NULL) return EXIT_SUCCESS;
        do {
                unsigned long wordlen=strlen(word);
                // If there is a macro, check if word matches macro
                if (macro_list_len > 0) for (int mli=0; mli < macro_list_len; mli++) {
                        macro* mp=&macro_list[mli];
                        if (strncmp(mp->name.data, word, wordlen) == 0) {
                                // Put macro expansion into temporary file, to be read and further expanded
                                //FILE* tmp=fopen("/tmp/sl/tmp.SL", "w");
                                sl_handle_err(sl_load_macro_args(mp, line),
                                        sl_error_msg("failed to load macro args");)
                                sl_handle_err(mp->replace(out, send_argc, send_argv),
                                        sl_error_msg("replace failed for %s", (char*) &mp->name);
                                        eprintf("Arguments:\n");
                                        for (int i=0; i < send_argc; i++) {
                                                eprintf("  (%d) %s\n", i, send_argv[i]);
                                        })
                                return EXIT_SUCCESS;
                        }
                }
                // If no macros are matched, put word, with a space
                fputs(word, out);
                putc(' ', out);
        } while ((word=strtok(NULL, " \n")) != NULL);
        putc('\n', out);

        return EXIT_SUCCESS;
}
#undef FAIL

int sl_read_macro(FILE* src, char* line)
#define FAIL return EXIT_FAILURE;
{
        assert(src != NULL);
        assert(line != NULL);
        assert(tmp_dir != NULL);

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
        sl_handle_err(temp == NULL)
        while (fgets(line, MAX_LINE-1, src) != NULL) {
                if (line[0] == '#' && line[1] == '#') {
                        macro* mp=&macro_list[macro_list_len-1];
                        sl_string tempso_name;
                        tempso_name.len=sprintf(tempso_name.data, "%s/%s.so", tmp_dir, mp->name.data);

                        sl_handle_err(fclose(temp))
                        pid_t p=fork();
                        if (p == 0) {
                                execvp("gcc", (char*[MAX_LINE]){"gcc", "-g", "-fPIC", "-shared", temp_name.data, "-o", tempso_name.data});
                        }
                        wait(NULL);
                        void* shared_object_handle=dlopen((char*) &tempso_name, RTLD_NOW);
                        sl_handle_err(shared_object_handle == NULL, sl_error_msg("%s failed to open", (char*) &tempso_name);)
                        mp->replace=dlsym(shared_object_handle, "replace");
                        sl_handle_err(mp->replace == NULL, sl_error_msg("\"replace\" label not found");)
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

void usage()
{
        printf("usage: sl file1 [file2] ...\n");
        exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
#define FAIL return EXIT_FAILURE;
{
        sl_handle_err(argc < 2, usage();)
        sl_handle_err(sl_generate(argv[1]) == NULL,
                sl_error_msg("failed to generate %s", argv[1]);)
        sl_handle_err(tmp_dir == NULL)
        sl_string as_file;
        sl_string o_file;
        as_file.len=sprintf(as_file.data, "%s/out.s", tmp_dir);
        o_file.len=sprintf(o_file.data, "%s/out.o", tmp_dir);
        pid_t p=fork();
        if (p == 0) {
                execvp("as", (char*[MAX_LINE]){"as", as_file.data, "-o", o_file.data});
        }
        sl_handle_err(wait(NULL) == -1, sl_error_msg("child process failed");)
        execvp("ld", (char*[MAX_LINE]){"ld", o_file.data, "-o", "a.out"});
}
#undef FAIL
