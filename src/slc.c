#include "SL/SL.h"
#include "SL/macros.h"
#include "SL/memory.h"
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

typedef struct sl_object {
        sl_string name;
        int (*prologue)();
        int (*replace)(FILE*, int argc, char** argv);
}* sl_object;
decl_sl_vector(sl_object)
impl_sl_vector(sl_object)

typedef struct line_node {
        u64 next;
        u64 prev;
        sl_string line;
        u64 lineno;
} line_node;
decl_sl_vector(line_node)
impl_sl_vector(line_node)

sl_arena GLOB;
sl_vector_sl_object OBJECT_LIST;
i32 send_argc;
char send_argv_buffer[MAX_ARGS][MAX_LINE];
cstring send_argv[MAX_ARGS];
const char* tmp_dir=NULL;
u64 verbose=0;

/* Generate assembly from SL code. */
errcode generate(cstring filename);
/* Load a file */
object load_file(object);
/* Loads an object function call into send_argc and send_argv. */
inline void load_object_args(sl_object mp, cstring line);
/* Processes a line of input. */
errcode process_line (sl_vector_line_node, u64 index);
/* Create a folder with subfolders. */
errcode install_folder(cstring file, __mode_t mode);
/* Reads an object definition. */
errcode read_object(FILE* src, cstring line);
/* Create executable from output code */
errcode render_executable(cstring out_path, cstring build_cpy_path);
/* Finds next non-whitespace index. */
inline i32 skip_whitespace(const char* line, int start);
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

        // Populate line vector with input
        sl_vector_line_node out_vec=sl_vector_new_line_node();
        line_node* out_vec_ptr=sl_vector_as_array_line_node(out_vec);
        sl_arena_push_back(GLOB, out_vec);
        char line[MAX_LINE];
        line_node new_line={0, 0, NULL, 0 };
        sl_vector_push_back_line_node(out_vec, new_line);
        tmp_dir=tmp_filedir->data;
        for (u64 i=1; fgets(line, MAX_LINE-1, in) != NULL; i++) {
                // Match lines starting with ## as macro definitions
                if (line[0] == '#' && line[1] == '#') {
                        sl_handle_err(read_object(in, line),
                                eprintf("%s:%lu: error: failed to read macro\n", filename, i);)
                } else if (strncmp(line, "load", 4) == 0) {
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
                        tmp_dir=tmp_filedir->data;
                } else {
                        const u64 pos=sl_vector_len(out_vec);
                        new_line=(line_node){0, pos - 1, sl_string_new(GLOB, line), i };
                        out_vec_ptr[new_line.prev].next=pos;
                        sl_vector_push_back_line_node(out_vec, new_line);
                }
        }
        sl_handle_err(fclose(in), sl_error_msg("fclose failed: %s", strerror(errno));)

        if (sl_vector_len(out_vec) == 1) return EXIT_SUCCESS;

        for (u64 i=1; i != 0; i=out_vec_ptr[i].next) {
                sl_handle_err(process_line(out_vec, i), eprintf("%s:%lu: error: failed to process line", filename, i);)
        }

        FILE* out=fopen(outfile_name->data, "w");
        sl_handle_err(out == NULL,
                eprintf("error: failed to open %s: %s\n", outfile_name->data, strerror(errno));)

        for (u64 i=1; i != 0; i=out_vec_ptr[i].next) {
                line_node* line_arr=sl_vector_as_array_line_node(out_vec);
                fputs(line_arr[i].line->data, out);
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

void load_object_args(sl_object mp, cstring line)
{
        assert(mp != NULL);
        assert(line != NULL);

        send_argc=0;
        cstring arg=line;
        do send_argv[send_argc++]=arg;
        while ((arg=strtok(NULL, " \n")) != NULL);
}

errcode process_line (sl_vector_line_node v, u64 index)
{
        FILE* tmp=tmpfile();
        assert(tmp != NULL);

        line_node *const nodes_arr=sl_vector_as_array_line_node(v);
        line_node *const start=&nodes_arr[index];
        const u64 lineno=start->lineno;
        char linec[MAX_LINE];
        strcpy(linec, start->line->data);
        cstring word=strtok(linec, " \n");
        if (word == NULL) return EXIT_SUCCESS;
        do {
                u64 wordlen=strlen(word);
                // If there is a macro, check if word matches macro
                if (sl_vector_len(OBJECT_LIST) > 0) for (int mli=0; mli < sl_vector_len(OBJECT_LIST); mli++) {
                        sl_object mp=sl_vector_as_array_sl_object(OBJECT_LIST)[mli];
                        if (cstr_eq(mp->name->data, word)) {
                                // Put macro expansion into temporary file, to be read and further expanded
                                load_object_args(mp, linec);
                                sl_handle_err(mp->replace(tmp, send_argc, send_argv),
                                        sl_error_msg("replace failed for %s", mp->name->data);
                                        eprintf("Arguments:\n");
                                        for (int i=0; i < send_argc; i++) {
                                                eprintf("  (%d) %s\n", i, send_argv[i]);
                                        })
                                goto write_lines;
                        }
                }
                // If no macros are matched, put word, with a space
                fputs(word, tmp);
                putc(' ', tmp);
        } while ((word=strtok(NULL, " \n")) != NULL);
        putc('\n', tmp);

write_lines:
        assert(fseek(tmp, 0, SEEK_SET) == EXIT_SUCCESS);
        // Replace input line with first line of output
        if (fgets(linec, MAX_LINE-1, tmp) != NULL) {
                start->line=sl_string_new(GLOB, linec);
        }
        // Add further line_node's to linked list
        u64 i=index;
        line_node* node_prev=start;
        while(fgets(linec, MAX_LINE-1, tmp) != NULL) {
                sl_vector_push_back_line_node(v,
                        (line_node) {
                                node_prev->next,
                                i,
                                sl_string_new(GLOB, linec),
                                lineno});
                i=node_prev->next=sl_vector_len(v) - 1;
                node_prev=&nodes_arr[i];
        }
        fclose(tmp);
        return EXIT_SUCCESS;
}

errcode read_object(FILE* src, cstring line)
{
        assert(src != NULL);
        assert(line != NULL);
        assert(tmp_dir != NULL);

        FILE* temp;
        // Create object
        sl_object new_sl_object=sl_arena_allocate(GLOB, sizeof(struct sl_object));
        errcode e;
        sl_handle_err(sl_vector_push_back_sl_object(OBJECT_LIST, new_sl_object))
        sl_string_builder sl_object_name=sl_arena_create_string_builder(GLOB);
        // Copy object name (skip "##" at the beginning)
        i32 i=skip_whitespace(line, 2);
        i32 j=0;
        while (isalnum(line[i]) || line[i] == '_') {
                sl_object_name->string[j++]=line[i++];
        }
        sl_object_name->len=j;
        new_sl_object->name=sl_string_builder_commit(sl_object_name);
        // Open object temp file
        sl_string temp_name=sl_string_format(GLOB, "%s/%s.c", tmp_dir, new_sl_object->name->data);
        sl_handle_err(e, sl_error_msg("failed to set filename for object: %s", new_sl_object->name->data);)
        temp=fopen(temp_name->data, "w");
        sl_handle_err(temp == NULL, sl_error_msg("could not open object file: %s", strerror(errno));)
        while (fgets(line, MAX_LINE-1, src) != NULL) {
                if (line[0] == '#' && line[1] == '#') {
                        sl_object m=sl_vector_as_array_sl_object(OBJECT_LIST)[sl_vector_len(OBJECT_LIST)-1];
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
        eprintf("error: object must end with \"##\"\n");
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

i32 skip_whitespace(const char* line, i32 start)
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
        OBJECT_LIST=sl_vector_new_sl_object();

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
