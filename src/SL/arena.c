#define SL_STRING_INTERNAL
#define SL_VECTOR_INTERNAL
#define SL_ARENA_INTERNAL
#include "memory.h"

#define FAIL return EXIT_FAILURE;
impl_sl_vector(sl_vector_base)

errcode sl_arena_new(sl_arena* arena)
{
        assert(arena != NULL);

        u64 size=GB(1);
        sl_vector_base mem=malloc(sizeof(struct sl_vector_base) + size);
        sl_vector_base vector_list=malloc(sizeof(struct sl_vector_base) + size);
        mem->size=size;
        mem->stride=1;
        mem->string.len=0;
        vector_list->size=size;
        vector_list->stride=sizeof(sl_vector_base);
        vector_list->string.len=sizeof(sl_vector_base);
        sl_vector_base* vector_list_data=(sl_vector_base*) vector_list->string.data;
        vector_list_data[0]=mem;
        *arena=(sl_arena) vector_list;
        return EXIT_SUCCESS;
}

errcode sl_arena_allocate(sl_arena arena, object* o, u64 bytes)
{
        sl_vector_base mem=*(sl_vector_base*)((sl_vector_base) arena)->string.data;
        *o=mem->string.data + mem->string.len;
        mem->string.len += bytes;
        return EXIT_SUCCESS;
}

sl_string_builder sl_arena_create_string_builder(sl_arena arena)
{
        sl_vector_base mem=*sl_vector_as_array_sl_vector_base(arena);
        return sl_vector_create_string_builder(mem);
}

errcode sl_arena_free(sl_arena arena)
{
        sl_vector_base vector_list=(sl_vector_base) arena;
        return EXIT_SUCCESS;
}

errcode sl_arena_reset(sl_arena arena)
{
        sl_vector_base vector_list=(sl_vector_base) arena;
        vector_list->string.len=0;
        return EXIT_SUCCESS;
}

errcode _sl_arena_push_back(sl_arena arena, sl_vector_base vec)
{
        sl_vector_sl_vector_base vector_list=(sl_vector_sl_vector_base) arena;
        sl_vector_push_back_sl_vector_base(vector_list, vec);
        return EXIT_SUCCESS;
}
