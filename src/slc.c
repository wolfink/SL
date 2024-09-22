#include "SL/SL.h"
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

// #define eprintf(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
// #define sl_error_msg(fmt,...) eprintf(__FILE__ ":%d: "__PRETTY_FUNCTION__ fmt "\n", __LINE__, ##__VA_ARGS__)
// #define sl_handle_err(expr,...) if(expr) { __VA_ARGS__ FAIL }
// #define MAX_LINE 256
// #define MAX_ARGS 256
// #define MAX_MACRO_NAME 64
// #define MAX_MACROS 1024

struct stat st={0};

// typedef struct string {
//         char data[MAX_LINE];
//         int len;
// } sl_string;

typedef struct macro {
        sl_string name;
        int (*prologue)();
        int (*replace)(FILE*, int argc, char** argv);
}* macro;
#define FAIL return EXIT_FAILURE;
decl_sl_vector(macro)
impl_sl_vector(macro)
#undef FAIL
// macro macro_list[MAX_MACROS];
sl_vector_macro MACRO_LIST;

int send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
char* send_argv[MAX_ARGS];

const char* tmp_dir=NULL;

/* Generate assembly from SL code. */
void* sl_generate(void*);
/* Loads a macro function call into send_argc and send_argv. */
int sl_load_macro_args(macro mp, char* line);
/* Processes a line of input. */
int sl_process_line (FILE* out, char* line);
/* Create a folder with subfolders. */
int sl_install(char* file, __mode_t mode);
/* Reads a macro definition. */
int sl_read_macro(FILE* src, char* line);
/* Finds next non-whitespace index. */
inline int sl_skip_whitespace(char* line, int start);

sl_arena GLOB;

void* sl_generate(void* vargp)
#define FAIL return NULL;
{
        assert(vargp != NULL);
        errcode e;

        char* filename=vargp;
        FILE* in=fopen(filename, "r");
        sl_handle_err(in == NULL,
                sl_error_msg("failed to open %s: %s", filename, strerror(errno));)
        char* filename_no_ext=strtok(filename, ".");
        sl_handle_err (filename_no_ext == NULL,
                sl_error_msg("filename has no body");)

        // Install tmp directories
        sl_string tmp_filedir;
        sl_string_format(GLOB, &tmp_filedir, "/tmp/sl/%s", filename_no_ext);
        //tmp_filedir.len=sprintf(tmp_filedir, "/tmp/sl/%s", filename_no_ext);
        sl_handle_err(sl_install((cstring) tmp_filedir->data, 0700) == EXIT_FAILURE,
                sl_error_msg("could not install %s", tmp_filedir->data);)

        sl_string outfile_name;
        sl_string_format(GLOB, &outfile_name, "%s/out.s", tmp_filedir->data);
        //outfile_name.len=sprintf(outfile_name, "%s/out.s", tmp_filedir.data);

        FILE* out=fopen(outfile_name->data, "w");
        sl_handle_err(out == NULL,
                sl_error_msg("failed to open %s: %s", outfile_name->data, strerror(errno));)

        char line[MAX_LINE];
        // Find a macro, copy its code into a c file
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                tmp_dir=tmp_filedir->data;
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
                        int last_slash=0;
                        for (int j=0; filename[j] != 0; j++) if (filename[j] == '/') last_slash=j;
                        filename[last_slash] = '\0';
                        sl_string dir;
                        e=sl_string_new(GLOB, &dir, filename);
                        sl_handle_err(e, sl_error_msg("could not create dir string");)
                        filename[last_slash] = '/';
                        sl_string load_file;
                        e=sl_string_format(GLOB, &load_file, "%s/%s", dir->data, load_filename);
                        sl_handle_err(e, sl_error_msg("could not format load_file string");)
                        pthread_t thread_id;
                        sl_handle_err(pthread_create(&thread_id, NULL, sl_generate, (void*) load_file->data))
                        sl_handle_err(pthread_join(thread_id, NULL));
                } else assert(sl_process_line(out, line) == EXIT_SUCCESS);
        }
        sl_handle_err(fclose(out), sl_error_msg("fclose failed: %s", strerror(errno));)
        return vargp;
}
#undef FAIL

errcode sl_install(char* directory, __mode_t mode)
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

int sl_load_macro_args(macro mp, char* line)
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
                if (sl_vector_len(MACRO_LIST) > 0) for (int mli=0; mli < sl_vector_len(MACRO_LIST); mli++) {
                        macro mp=sl_vector_as_array_macro(MACRO_LIST)[mli];
                        if (cstr_eq(mp->name->data, word)) {
                                // Put macro expansion into temporary file, to be read and further expanded
                                //FILE* tmp=fopen("/tmp/sl/tmp.SL", "w");
                                sl_handle_err(sl_load_macro_args(mp, line),
                                        sl_error_msg("failed to load macro args");)
                                sl_handle_err(mp->replace(out, send_argc, send_argv),
                                        sl_error_msg("replace failed for %s", mp->name->data);
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

        FILE* temp;
        // Create macro
        macro new_macro;
        sl_arena_allocate(GLOB, (object*) &new_macro, sizeof(struct macro));
        sl_vector_push_back_macro(MACRO_LIST, new_macro);
        sl_string_builder macro_name=sl_arena_create_string_builder(GLOB);
        // Copy macro name (skip "##" at the beginning)
        int i=sl_skip_whitespace(line, 2);
        int j=0;
        while (isalnum(line[i]) || line[i] == '_') {
                macro_name->string[j++]=line[i++];
        }
        macro_name->len=j;
        new_macro->name=sl_string_builder_commit(macro_name);
        // Open macro temp file
        sl_string temp_name;
        errcode e=sl_string_format(GLOB, &temp_name, "%s/%s.c", tmp_dir, new_macro->name->data);
        sl_handle_err(e, sl_error_msg("failed to set filename for macro: %s", new_macro->name->data);)
        temp=fopen(temp_name->data, "w");
        sl_handle_err(temp == NULL, sl_error_msg("could not open macro file: %s", strerror(errno));)
        while (fgets(line, MAX_LINE-1, src) != NULL) {
                if (line[0] == '#' && line[1] == '#') {
                        macro m=sl_vector_as_array_macro(MACRO_LIST)[sl_vector_len(MACRO_LIST)-1];
                        sl_string tempso_name;
                        sl_string_format(GLOB, &tempso_name, "%s/%s.so", tmp_dir, m->name->data);

                        sl_handle_err(fclose(temp), sl_error_msg("failed to close file: %s", temp_name->data);)
                        pid_t p=fork();
                        if (p == 0) {
                                const cstring command[]={"gcc", "-g", "-fPIC", "-shared", (char*) temp_name->data, "-o", (char*) tempso_name->data, NULL};
                                e=execvp(command[0], command);
                                sl_handle_err(e, 
                                        sl_error_msg("failed to execute command (%s):", strerror(errno));
                                        eprintf("    ");
                                        for (int i=0; i < sizeof(command) / sizeof(cstring); i++) eprintf("%s ", command[i]);
                                        eprintf("\n");)
                                
                        }
                        wait(NULL);
                        void* shared_object_handle=dlopen(tempso_name->data, RTLD_NOW);
                        sl_handle_err(shared_object_handle == NULL, sl_error_msg("%s failed to open", tempso_name->data);)
                        m->replace=dlsym(shared_object_handle, "replace");
                        sl_handle_err(m->replace == NULL, sl_error_msg("\"replace\" label not found");)
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
        errcode e=sl_arena_new(&GLOB);
        sl_handle_err(e, sl_error_msg("failed to created global arena");)
        e=sl_vector_new_macro(&MACRO_LIST);
        sl_handle_err(e, sl_error_msg("failed to created macro list");)
        sl_handle_err(argc < 2, usage();)
        sl_handle_err(sl_generate(argv[1]) == NULL,
                sl_error_msg("failed to generate %s", argv[1]);)
        sl_handle_err(tmp_dir == NULL)
        sl_string as_file;
        sl_string o_file;
        sl_string_format(GLOB, &as_file, "%s/out.s", tmp_dir);
        sl_string_format(GLOB, &o_file, "%s/out.o", tmp_dir);
        pid_t p=fork();
        if (p == 0) {
                char* command[]={"as", (char*) as_file->data, "-o", (char*) o_file->data};
                execvp(command[0], command);
        }
        sl_handle_err(wait(NULL) == -1, sl_error_msg("child process failed");)
        char* command[]={"ld", (char*) o_file->data, "-o", "a.out"};
        execvp(command[0], command);
}
#undef FAIL
