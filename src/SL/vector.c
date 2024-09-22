#define SL_VECTOR_INTERNAL
#define SL_STRING_INTERNAL

#include "memory.h"

const u64 VEC_SIZE=GB(1);

#define FAIL return EXIT_FAILURE;

#define SL_TYPE(_,type) impl_sl_vector(type)
        type_table
#undef SL_TYPE

sl_vector_base sl_vector_base_new(u64 stride)
{
        sl_vector_base vector;
        vector=malloc(sizeof(struct sl_vector_base) + VEC_SIZE);
        assert(vector != NULL);

        vector->size=VEC_SIZE;
        vector->stride=stride;
        vector->string.len=0;
        return vector;
}

i32 sl_vector_base_write(sl_vector_base vector, u64 N, cstring bytes)
{
        assert(vector->string.len <= vector->size);
        memcpy(vector->string.data + vector->string.len, bytes, N);
        vector->string.len += N;
        return N;
}

cstring sl_vector_base_pop(sl_vector_base vector, u64 N)
{
        assert(vector->string.len <= vector->size);
        vector->string.len -= N;
        return &vector->string.data[vector->string.len];
}

cstring sl_vector_base_get_data(sl_vector_base vector)
{
        assert(vector != NULL);
        return vector->string.data;
}

// TODO: lock down vector while string builder is active
sl_string_builder sl_vector_create_string_builder(sl_vector_base vector)
{
        assert(vector != NULL);
        assert(vector->stride == 1);

        sl_string_builder sb;
        sb=malloc(sizeof(struct sl_string_builder));
        assert(sb != NULL);

        sb->view.size=vector->size - vector->string.len;
        sb->view.stride=1;
        sb->view.view.len=0;
        sb->view.view.data_addr=&vector->string.data[vector->string.len + sizeof(struct sl_string)];
        sb->vector=vector;
        return sb;
}

// TODO: release vector after free'ing string builder
sl_string sl_string_builder_commit(sl_string_builder sb)
{
        assert(sb != NULL);
        assert(sb->vector != NULL);

        sl_string s=(sl_string) &sb->vector->string.data[sb->vector->string.len];
        sb->vector->string.len += sb->view.view.len + sizeof(struct sl_string) + 1;
        s->len=sb->view.view.len;
        s->data[s->len]='\0';
        free(sb);
        return s;
}
