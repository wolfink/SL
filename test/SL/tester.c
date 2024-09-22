#include <stdlib.h>
#define SL_VECTOR_INTERNAL
#define SL_ARENA_INTERNAL
#define SL_STRING_INTERNAL
#include "SL/memory.h"
#include <SL/SL.h>

#define sl_vector_len_internal(v) (((sl_vector_base) v)->string.len / ((sl_vector_base) v)->stride)

#define FAIL return EXIT_FAILURE;
errcode test_vector()
{
        sl_vector_i32 vector;
        errcode e=sl_vector_new_i32(&vector);
        e |= sl_vector_push_back_i32(vector, 1);
        e |= sl_vector_push_back_i32(vector, 2);
        e |= sl_vector_push_back_i32(vector, 3);
        assert(sl_vector_len_internal(vector) == 3);
        i32 value;
        e=sl_vector_pop_i32(vector, &value);
        assert(value == 3);
        e |= sl_vector_pop_i32(vector, &value);
        assert(value == 2);
        e |= sl_vector_push_back_i32(vector, -1);
        e |= sl_vector_push_back_i32(vector, -3);
        e |= sl_vector_push_back_i32(vector, 4);
        assert(sl_vector_len_internal(vector) == 4);
        i32* a=sl_vector_as_array_i32(vector);
        assert(a[0] == 1);
        assert(a[1] == -1);
        assert(a[2] == -3);
        assert(a[3] == 4);
        assert(e == EXIT_SUCCESS);
        return EXIT_SUCCESS;
}

errcode test_arena()
{
        sl_arena arena;
        errcode e=sl_arena_new(&arena);
        i64* array;
        e |= sl_arena_allocate(arena, (void**) &array, sizeof(i64) * 32);
        for (int i=0, val=1; i < 32; i++, val *= 2) array[i]=val;
        sl_vector_base mem=sl_vector_as_array_sl_vector_base(arena)[0];
        for (int i=0, val=1; i < 32; i++, val *= 2) assert(((i64*) mem->string.data)[i] == val);
        sl_vector_i64 v;
        e |= sl_vector_new_i64(&v);
        for (int i=0, val=1; i < 32; i++, val *= 2) e |= sl_vector_push_back_i64(v, val);
        sl_arena_push_back(arena, v);
        assert(v == (sl_vector_i64) sl_vector_as_array_sl_vector_base(arena)[1]);
        assert(e == EXIT_SUCCESS);
        return EXIT_SUCCESS;
}

errcode test_string()
{
        sl_arena arena;
        errcode e=sl_arena_new(&arena);
        sl_string str1;
        e |= sl_string_new(arena, &str1, "This is a string!");
        printf("str1: (%lu, \"%s\")\n", str1->len, str1->data);
        sl_string str2;
        e |= sl_string_copy(arena, &str2, str1);
        printf("str2: (%lu, \"%s\")\n", str2->len, str2->data);
        sl_string_builder str3_builder=sl_arena_create_string_builder(arena);
        u64* len=&str3_builder->view.view.len;
        cstring cstr3=str3_builder->view.view.data_addr;
        *len=sprintf(cstr3, "%s The third to be exact...", str2->data);
        sl_string str3=sl_string_builder_commit(str3_builder);
        printf("str3: (%lu, \"%s\")\n", str3->len, str3->data);
        sl_string str4;
        e |= sl_string_format(arena, &str4, "str1: %s, str2: %s, str3: %s", str1->data, str2->data, str3->data);
        cstring test="str1: This is a string!, str2: This is a string!, str3: This is a string! The third to be exact...";
        assert(cstr_eq(str4->data, "str1: This is a string!, str2: This is a string!, str3: This is a string! The third to be exact..."));
        assert(e == EXIT_SUCCESS);
        printf("Total memory used: %lu\nMaximum memory allowed: %lu\n",
                sl_vector_as_array_sl_vector_base(arena)[0]->string.len,
                sl_vector_as_array_sl_vector_base(arena)[0]->size);
        return EXIT_SUCCESS;
}

errcode main(i32 argc, cstring* argv)
{
        assert(argc == 2);
        char* test=argv[1];
        if (cstr_eq(test, "vector")) return test_vector();
        else if (cstr_eq(test, "arena")) return test_arena();
        else if (cstr_eq(test, "string")) return test_string();
        return EXIT_FAILURE;
}
