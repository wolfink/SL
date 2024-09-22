#define SL_STRING_INTERNAL

#include <stdarg.h>
#include <stdlib.h>
#include "macros.h"
#include "memory.h"

#define FAIL return EXIT_FAILURE;
errcode sl_string_new(sl_arena arena, sl_string* dest, cstring src)
{
        assert(arena != NULL);
        assert(src != NULL);

        u64 len=strlen(src);
        sl_handle_err(sl_arena_allocate(arena, (void*) dest, len + sizeof(struct sl_string) + 1),
                sl_error_msg("failed to allocate memory from arena");)
        sl_handle_err(strcpy((*dest)->data, src) == NULL,
                sl_error_msg("failed to copy string data");)
        (*dest)->len=len;
        (*dest)->data[(*dest)->len]='\0';
        return EXIT_SUCCESS;
}

errcode sl_string_copy(sl_arena arena, sl_string* dest, sl_string src)
{
        assert(arena != NULL);
        assert(src != NULL);

        sl_handle_err(sl_arena_allocate(arena, (void*) dest, src->len + sizeof(struct sl_string) + 1),
                sl_error_msg("failed to allocate memory from arena");)
        sl_handle_err(strcpy((*dest)->data, src->data) == NULL,
                sl_error_msg("failed to copy string data");)
        (*dest)->len=src->len;
        (*dest)->data[(*dest)->len]='\0';
        return EXIT_SUCCESS;
}

cstring sl_string_as_cstring(sl_string str)
{
        return str->data;
}

errcode sl_string_format(sl_arena arena, sl_string* dest, cstring fmt, ...)
{
        assert(arena != NULL);
        assert(fmt != NULL);

        errcode e;
        sl_vector_base mem;
        e=sl_vector_base_new(&mem, 1);
        sl_string_builder sb=sl_vector_create_string_builder(mem);
        sl_handle_err(e, sl_error_msg("failed to create vector");)

        va_list args;
        va_start(args, fmt);
        sb->len=vsprintf(sb->string, fmt, args);
        va_end(args);
        *dest=sl_string_builder_commit(sb);
        sl_arena_push_back(arena, mem);
        return EXIT_SUCCESS;
}

errcode sl_create_string_view(sl_arena arena, sl_string_view* view, sl_string base, u64 start, u64 end)
{
        assert(arena != NULL);
        assert(base != NULL);
        assert(start < end);

        errcode e=sl_arena_allocate(arena, (void*) view, sizeof(struct sl_string_view));
        sl_handle_err(e, sl_error_msg("failed to allocate string view");)
        view->len=end - start;
        view->data_addr=base->data + start;
        return EXIT_SUCCESS;
}
