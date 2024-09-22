#ifndef SL_MEMORY_H
#define SL_MEMORY_H

#include "includes.h"
#include "macros.h"
#include "types.h"

//////////////////////////////////////////////////
/// String Declaration
//////////////////////////////////////////////////

#ifdef SL_STRING_INTERNAL

typedef struct sl_string {
        u64 len;
        char data[];
}* sl_string;

typedef struct sl_string_view {
        u64 len;
        cstring data_addr;
}* sl_string_view;

#else

typedef struct sl_string {
        const u64 len;
        const char data[];
}* sl_string;

typedef struct sl_string_view {
        const u64 len;
        const char* data_addr;
}* sl_string_view;

#endif

//////////////////////////////////////////////////
/// Vector Declaration
//////////////////////////////////////////////////

#ifdef SL_VECTOR_INTERNAL

typedef struct sl_vector_base {
        u64 size;
        u64 stride;
        struct sl_string string;
}* sl_vector_base;

typedef struct sl_vector_view {
        u64 size;
        u64 stride;
        struct sl_string_view view;
}* sl_vector_view;

typedef struct sl_string_builder {
        sl_vector_base vector;
        struct sl_vector_view view;
}* sl_string_builder;

#else

typedef struct sl_vector_base {
        const u8 _0[sizeof(u64)];
        const u64 _stride;
        const u64 _bytes;
}* sl_vector_base;

typedef struct sl_vector_view {
        const u8 _0[sizeof(u64)];
        const u64 _stride;
        const u64 _bytes;
}* sl_vector_view;

typedef struct sl_string_builder {
        const u8 _0[sizeof(sl_vector_base)];
        const u64 size;
        const u64 stride;
        u64 len;
        cstring string;
}* sl_string_builder;

#endif

//////////////////////////////////////////////////
/// Vector Macros
//////////////////////////////////////////////////

#define decl_sl_vector_new(type)\
sl_vector_##type sl_vector_new_##type()
#define impl_sl_vector_new(type) decl_sl_vector_new(type)\
{\
        return (sl_vector_##type) sl_vector_base_new(sizeof(type));\
}

#define decl_sl_vector_push_back(type)\
errcode sl_vector_push_back_##type(sl_vector_##type vector, type val)
#define impl_sl_vector_push_back(type) decl_sl_vector_push_back(type)\
{\
        assert(vector != NULL);\
        sl_vector_base base=(sl_vector_base) vector;\
        sl_handle_err(sl_vector_base_write(base, sizeof(type), (cstring) &val) != sizeof(type),\
                sl_error_msg("err'd in growing vector");)\
        return EXIT_SUCCESS;\
}

#define decl_sl_vector_pop(type)\
errcode sl_vector_pop_##type(sl_vector_##type vector, type* valp)
#define impl_sl_vector_pop(type) decl_sl_vector_pop(type)\
{\
        assert(valp != NULL);\
        assert(vector != NULL);\
        sl_vector_base base=(sl_vector_base) vector;\
        cstring popped=sl_vector_base_pop(base, sizeof(type));\
        sl_handle_err(popped == NULL,\
                sl_error_msg("err'd in shrinking vector");)\
        *valp=*(type*) popped;\
        return EXIT_SUCCESS;\
}

#define decl_sl_vector_as_array(type)\
type* sl_vector_as_array_##type(sl_vector_##type vector)
#define impl_sl_vector_as_array(type) decl_sl_vector_as_array(type)\
{\
        assert(vector != NULL);\
        return (type*) sl_vector_base_get_data((sl_vector_base) vector);\
}


#define decl_sl_vector(type)\
typedef struct {}* sl_vector_##type;\
decl_sl_vector_new(type);\
decl_sl_vector_push_back(type);\
decl_sl_vector_pop(type);\
decl_sl_vector_as_array(type);

#define impl_sl_vector(type)\
impl_sl_vector_new(type)\
impl_sl_vector_push_back(type)\
impl_sl_vector_pop(type)\
impl_sl_vector_as_array(type)

#define sl_vector_len(v) (((sl_vector_base) v)->_bytes / ((sl_vector_base) v)->_stride)

//////////////////////////////////////////////////
/// Vector Functions
//////////////////////////////////////////////////

extern sl_vector_base sl_vector_base_new(u64 stride);
extern i32 sl_vector_base_write(sl_vector_base, u64 N, cstring bytes);
extern cstring sl_vector_base_pop(sl_vector_base, u64 N);
extern cstring sl_vector_base_get_data(sl_vector_base);

extern sl_string_builder sl_vector_create_string_builder(sl_vector_base);
extern sl_string sl_string_builder_commit(sl_string_builder);

#define SL_TYPE(_,type) decl_sl_vector(type)
        type_table
#undef SL_TYPE

//////////////////////////////////////////////////
/// Arena Declaration
//////////////////////////////////////////////////

#ifdef SL_ARENA_INTERNAL
decl_sl_vector(sl_vector_base)
typedef sl_vector_sl_vector_base sl_arena;
#else
typedef struct {}* sl_arena;
#endif

//////////////////////////////////////////////////
/// String Functions
//////////////////////////////////////////////////

extern sl_string sl_string_new(sl_arena, cstring src);
extern sl_string sl_string_copy(sl_arena, sl_string src);
extern cstring sl_string_as_cstring(sl_string);
extern sl_string sl_string_format(sl_arena, cstring fmt, ...);
extern sl_string_view sl_create_string_view(sl_arena, sl_string base_string, u64 start, u64 end);

//////////////////////////////////////////////////
/// Arena Functions
//////////////////////////////////////////////////

extern sl_arena sl_arena_new();
extern object sl_arena_allocate(sl_arena, u64 bytes);
extern sl_string_builder sl_arena_create_string_builder(sl_arena);
extern void sl_arena_free(sl_arena);
extern void sl_arena_reset(sl_arena);
extern errcode _sl_arena_push_back(sl_arena, sl_vector_base);
#define sl_arena_push_back(arena, vec) _sl_arena_push_back(arena, (sl_vector_base) vec)

#endif
