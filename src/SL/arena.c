#define SL_STRING_INTERNAL
#define SL_VECTOR_INTERNAL
#define SL_ARENA_INTERNAL
#include "memory.h"

impl_sl_vector(sl_vector_base)

sl_arena sl_arena_new()
{
        u64 size=MB(1);
        sl_vector_base mem=malloc(sizeof(struct sl_vector_base) + size);
        sl_vector_base vector_list=malloc(sizeof(struct sl_vector_base) + size);
        assert(mem != NULL);
        assert(vector_list != NULL);

        mem->size=size;
        mem->stride=1;
        mem->string.len=0;
        vector_list->size=size;
        vector_list->stride=sizeof(sl_vector_base);
        vector_list->string.len=sizeof(sl_vector_base);
        sl_vector_base* vector_list_data=(sl_vector_base*) vector_list->string.data;
        vector_list_data[0]=mem;
        return (sl_arena) vector_list;
}

object sl_arena_allocate(sl_arena arena, u64 bytes)
{
        object o;
        sl_vector_base mem=*(sl_vector_base*)((sl_vector_base) arena)->string.data;
        assert(mem->string.len + bytes <= mem->size);

        o=mem->string.data + mem->string.len;
        mem->string.len += bytes;
        return o;
}

sl_string_builder sl_arena_create_string_builder(sl_arena arena)
{
        sl_vector_base mem=*sl_vector_as_array_sl_vector_base(arena);
        return sl_vector_create_string_builder(mem);
}

void sl_arena_free(sl_arena arena)
{
        const sl_vector_base vector_list=(sl_vector_base) arena;
        const u64 num_vectors=vector_list->string.len / vector_list->stride;
        sl_vector_base const* vectors=(sl_vector_base*) vector_list->string.data;
        for (u64 i=0; i < num_vectors; i++) {
                free(vectors[i]);
        }
        free(arena);
}

void sl_arena_reset(sl_arena arena)
{
        const sl_vector_base vector_list=(sl_vector_base) arena;
        vector_list->string.len=0;
}

errcode _sl_arena_push_back(sl_arena arena, sl_vector_base vec)
{
        const sl_vector_sl_vector_base vector_list=(sl_vector_sl_vector_base) arena;
        return sl_vector_push_back_sl_vector_base(vector_list, vec);
}
