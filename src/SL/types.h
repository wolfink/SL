#ifndef TYPES_H
#define TYPES_H
//#include "macros.h"
#include <stdint.h>

#define type_table\
        SL_TYPE(char,           c8)\
        SL_TYPE(int8_t,         i8)\
        SL_TYPE(int16_t,        i16)\
        SL_TYPE(int32_t,        i32)\
        SL_TYPE(int64_t,        i64)\
        SL_TYPE(uint8_t,        u8)\
        SL_TYPE(uint16_t,       u16)\
        SL_TYPE(uint32_t,       u32)\
        SL_TYPE(uint64_t,       u64)\
        SL_TYPE(float,          f32)\
        SL_TYPE(double,         f64)\
        SL_TYPE(char*,          cstring)\
        SL_TYPE(void*,          object)

#define SL_TYPE(original, new)\
        typedef original new;
        type_table
#undef SL_TYPE

typedef i32 errcode;

//#define SAL_TYPE(_, name)\
//        impl_option(name);
//        type_table
//#undef SAL_TYPE

#endif
