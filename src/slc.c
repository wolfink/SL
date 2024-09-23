#include "SL/SL.h"
#include "SL/macros.h"
#include "SL/types.h"
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

typedef struct macro {
        sl_string name;
        int (*prologue)();
        int (*replace)(FILE*, int argc, char** argv);
}* macro;
decl_sl_vector(macro)
impl_sl_vector(macro)

sl_arena GLOB;
sl_vector_macro MACRO_LIST;
i32 send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
cstring send_argv[MAX_ARGS];
const char* tmp_dir=NULL;
u64 verbose=0;

/* Load a file */
object load_file(object);
/* Generate assembly from SL code. */
errcode generate(cstring filename);
/* Loads a macro function call into send_argc and send_argv. */
inline void load_macro_args(macro mp, cstring line);
/* Processes a line of input. */
errcode process_line (FILE* out, cstring line);
/* Create a folder with subfolders. */
errcode install_folder(cstring file, __mode_t mode);
/* Reads a macro definition. */
errcode read_macro(FILE* src, cstring line);
/* Create executable from output code */
errcode render_executable(cstring out_path, cstring build_cpy_path);
/* Finds next non-whitespace index. */
inline i32 skip_whitespace(char* line, int start);
/* Display usage message */
void usage(void);

errcode generate(cstring filename)
{
        assert(filename != NULL);

        FILE* in=fopen(filename, "r");
        sl_handle_err(in == NULL,
                eprintf("error: failed to open %s: %s\n", filename, strerror(errno));)
        char* filename_no_ext=strtok(filename, ".");
        sl_handle_err (filename_no_ext == NULL,
                eprintf("error: filename has no body\n");)

        // Install tmp directories
        sl_string tmp_filedir=sl_string_format(GLOB, "/tmp/sl/%s", filename_no_ext);
        sl_handle_err(install_folder((cstring) tmp_filedir->data, 0700) == EXIT_FAILURE,
                eprintf("error: could not install %s\n", tmp_filedir->data);)

        sl_string outfile_name=sl_string_format(GLOB, "%s/out.s", tmp_filedir->data);

        FILE* out=fopen(outfile_name->data, "w");
        sl_handle_err(out == NULL,
                eprintf("error: failed to open %s: %s\n", outfile_name->data, strerror(errno));)

        char line[MAX_LINE];
        i32 lineno=1;
        // Find a macro, copy its code into a c file
        while (fgets(line, MAX_LINE-1, in) != NULL) {
                tmp_dir=tmp_filedir->data;
                // Match lines starting with ## as macro definitions
                if (line[0] == '#' && line[1] == '#') {
                        sl_handle_err(read_macro(in, line),
                                eprintf("%s:%d: error: failed to read macro\n", filename, lineno);)
                }
                // Match lines that start with "load" as macro loading
                else if (strncmp(line, "load", 4) == 0) {
                        i32 i=skip_whitespace(line, 4);
                        sl_handle_err(line[i] != '\"', sl_error_msg("expected \'\"\'");)
                        char* load_filenamec=strtok(line + i + 1, "\"");
                        i32 last_slash=0;
                        for (i32 j=0; filename[j] != 0; j++) if (filename[j] == '/') last_slash=j;
                        filename[last_slash] = '\0';
                        sl_string dir=sl_string_new(GLOB, filename);
                        filename[last_slash] = '/';
                        sl_string load_filename=sl_string_format(GLOB, "%s/%s", dir->data, load_filenamec);
                        pthread_t thread_id;
                        sl_handle_err(pthread_create(&thread_id, NULL, load_file, (void*) load_filename->data),
                                sl_error_msg("failed to create pthread");)
                        sl_handle_err(pthread_join(thread_id, NULL),
                                sl_error_msg("failed to join pthread");)
                } else assert(process_line(out, line) == EXIT_SUCCESS);
                lineno++;
        }
        sl_handle_err(fclose(out), sl_error_msg("fclose failed: %s", strerror(errno));)
        return EXIT_SUCCESS;
}

struct stat ST={0};
errcode install_folder(cstring directory, __mode_t mode)
{
        assert(directory != NULL);

        for (i32 i=0; directory[i] != '\0'; i++) {
                if (directory[i] == '/' && i > 0) {
                        directory[i] = '\0';
                        if (stat(directory, &ST) == -1) {
                                sl_handle_err(mkdir(directory, mode),
                                        sl_error_msg("failed to create directory %s: %s\n", directory, strerror(errno));)
                        }
                        directory[i]='/';
                }
        }
        if (stat(directory, &ST) == -1) {
                sl_handle_err(mkdir(directory, mode),
                        eprintf("error: failed to create directory %s: %s\n", directory, strerror(errno));)
        }
        return EXIT_SUCCESS;
}

object load_file(object vargp)
{
        if (generate((cstring) vargp) == EXIT_FAILURE) return NULL;
        return vargp;
}

void load_macro_args(macro mp, cstring line)
{
        assert(mp != NULL);
        assert(line != NULL);

        send_argc=0;
        cstring arg=line;
        do send_argv[send_argc++]=arg;
        while ((arg=strtok(NULL, " \n")) != NULL);
}

errcode process_line (FILE* out, cstring line)
{
        assert(out != NULL);
        assert(line != NULL);

        cstring word=strtok(line, " \n");
        if (word == NULL) return EXIT_SUCCESS;
        do {
                u64 wordlen=strlen(word);
                // If there is a macro, check if word matches macro
                if (sl_vector_len(MACRO_LIST) > 0) for (int mli=0; mli < sl_vector_len(MACRO_LIST); mli++) {
                        macro mp=sl_vector_as_array_macro(MACRO_LIST)[mli];
                        if (cstr_eq(mp->name->data, word)) {
                                // Put macro expansion into temporary file, to be read and further expanded
                                load_macro_args(mp, line);
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

errcode read_macro(FILE* src, cstring line)
{
        assert(src != NULL);
        assert(line != NULL);
        assert(tmp_dir != NULL);

        FILE* temp;
        // Create macro
        macro new_macro=sl_arena_allocate(GLOB, sizeof(struct macro));
        errcode e;
        sl_handle_err(sl_vector_push_back_macro(MACRO_LIST, new_macro))
        sl_string_builder macro_name=sl_arena_create_string_builder(GLOB);
        // Copy macro name (skip "##" at the beginning)
        i32 i=skip_whitespace(line, 2);
        i32 j=0;
        while (isalnum(line[i]) || line[i] == '_') {
                macro_name->string[j++]=line[i++];
        }
        macro_name->len=j;
        new_macro->name=sl_string_builder_commit(macro_name);
        // Open macro temp file
        sl_string temp_name=sl_string_format(GLOB, "%s/%s.c", tmp_dir, new_macro->name->data);
        sl_handle_err(e, sl_error_msg("failed to set filename for macro: %s", new_macro->name->data);)
        temp=fopen(temp_name->data, "w");
        sl_handle_err(temp == NULL, sl_error_msg("could not open macro file: %s", strerror(errno));)
        while (fgets(line, MAX_LINE-1, src) != NULL) {
                if (line[0] == '#' && line[1] == '#') {
                        macro m=sl_vector_as_array_macro(MACRO_LIST)[sl_vector_len(MACRO_LIST)-1];
                        sl_string tempso_name=sl_string_format(GLOB, "%s/%s.so", tmp_dir, m->name->data);

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
        eprintf("error: macro must end with \"##\"\n");
        return EXIT_FAILURE;
}

errcode render_executable(cstring out_path, cstring build_cpy_path)
{
        errcode e;
        sl_string as_file;
        sl_string o_file;
        as_file=sl_string_format(GLOB, "%s/out.s", tmp_dir);
        o_file=sl_string_format(GLOB, "%s/out.o", tmp_dir);
        pid_t p=fork();
        if (p == 0) {
                cstring command[]={"as", (cstring) as_file->data, "-o", (cstring) o_file->data, NULL};
                sl_handle_err(execvp(command[0], command),
                        sl_error_msg("failed to execute command (%s):", strerror(errno));
                        for (i32 i=0; i < sizeof(command) / sizeof(cstring); i++) eprintf("%s ", command[i]);
                        eprintf("\n");)
        }
        sl_handle_err(wait(NULL) == -1, sl_error_msg("child process failed");)
        p=fork();
        if (p == 0) {
                cstring command[]={"ld", (char*) o_file->data, "-o", out_path, NULL};
                sl_handle_err(execvp(command[0], command),
                        sl_error_msg("failed to execute command (%s):", strerror(errno));
                        for (i32 i=0; i < sizeof(command) / sizeof(cstring); i++) eprintf("%s ", command[i]);
                        eprintf("\n");)
        }
        sl_handle_err(wait(NULL) == -1, sl_error_msg("child process failed");)
        if (build_cpy_path) {
                pid_t p=fork();
                if (p == 0) {
                        cstring command[]={"cp", "-r", "/tmp/sl", build_cpy_path, NULL};
                        errcode _=execvp(command[0], command);
                        sl_error_msg("failed to execute command (%s):", strerror(errno));
                        for (i32 i=0; i < sizeof(command) / sizeof(cstring); i++) eprintf("%s ", command[i]);
                        eprintf("\n");
                }
                sl_handle_err(wait(NULL) == -1, sl_error_msg("child process failed");)
        }
        cstring command[]={"rm", "-rf", "/tmp/sl", NULL};
        return execvp(command[0], command);
}

i32 skip_whitespace(cstring line, i32 start)
{
        i32 i=start - 1;
        while(iswspace(line[++i]));
        return i;
}

void usage()
{
        printf("Usage: sl [OPTION] FILES ...\n");
        exit(EXIT_FAILURE);
}

errcode main(i32 argc, cstring* argv)
{
        // Initialize constants
        GLOB=sl_arena_new();
        MACRO_LIST=sl_vector_new_macro();

        errcode e;
        cstring out_path="a.out";
        cstring build_cpy_path=NULL;

        // Parse arguments
        if (argc < 2) usage();
        for (i32 i=1; i < argc; i++) {
                if (cstr_eq(argv[i], "-o")) out_path=argv[++i];
                else if (cstr_eq(argv[i], "-v")) verbose=1;
                else if (cstr_eq(argv[i], "--copy-build-files")) build_cpy_path=argv[++i];
                else if (argv[i][0] == '-') eprintf("error: option \"%s\" not recognized\n", argv[i]);
                else sl_handle_err(load_file(argv[i]) == NULL,
                        eprintf("error: failed to load %s\n", argv[i]);)
        }
        assert(tmp_dir != NULL);
        return render_executable(out_path, build_cpy_path);
}
