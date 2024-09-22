#define SL_STRING_INTERNAL

#include <stdarg.h>
#include <stdlib.h>
#include "macros.h"
#include "memory.h"

sl_string sl_string_new(sl_arena arena, cstring src)
{
        assert(arena != NULL);
        assert(src != NULL);

        sl_string dest;
        u64 len=strlen(src);
        dest=sl_arena_allocate(arena, len + sizeof(struct sl_string) + 1);
        assert(strcpy(dest->data, src) != NULL);
        dest->len=len;
        dest->data[dest->len]='\0';
        return dest;
}

sl_string sl_string_copy(sl_arena arena, sl_string src)
{
        assert(arena != NULL);
        assert(src != NULL);

        sl_string dest;
        dest=sl_arena_allocate(arena, src->len + sizeof(struct sl_string) + 1);
        assert(strcpy(dest->data, src->data) != NULL);
        dest->len=src->len;
        dest->data[dest->len]='\0';
        return dest;
}

cstring sl_string_as_cstring(sl_string str)
{
        return str->data;
}

sl_string sl_string_format(sl_arena arena, cstring fmt, ...)
{
        assert(arena != NULL);
        assert(fmt != NULL);

        errcode e;
        sl_vector_base mem=sl_vector_base_new(1);
        sl_string_builder sb=sl_vector_create_string_builder(mem);
        assert(mem != NULL);
        assert(sb != NULL);
        sl_string dest;

        va_list args;
        va_start(args, fmt);
        sb->len=vsprintf(sb->string, fmt, args);
        assert(sb->string != NULL);
        va_end(args);
        dest=sl_string_builder_commit(sb);
        sl_arena_push_back(arena, mem);
        return dest;
}

sl_string_view sl_create_string_view(sl_arena arena, sl_string base, u64 start, u64 end)
{
        assert(arena != NULL);
        assert(base != NULL);
        assert(start < end);

        sl_string_view view;
        view=(sl_string_view) sl_arena_allocate(arena, sizeof(struct sl_string_view));
        view->len=end - start;
        view->data_addr=base->data + start;
        return EXIT_SUCCESS;
}
