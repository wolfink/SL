#ifndef MACROS_H
#define MACROS_H

#define MAX_LINE 256
#define MAX_ARGS 256
#define MAX_MACRO_NAME 64
#define MAX_MACROS 1024

#define cstr_eq(a,b) strcmp(a,b) == 0
#define cstr_lt(a,b) strcmp(a,b) < 0
#define cstr_gt(a,b) strcmp(a,b) > 0

#define eprintf(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define sl_error_msg(fmt,...) eprintf(__FILE__ ":%d: %s: " fmt "\n", __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)
#define sl_handle_err(expr,...) if(expr) { __VA_ARGS__ return (errcode) EXIT_FAILURE; }
#define verbose_printf(fmt,...) (FLAGS & 1)? printf(fmt, ##__VA_ARGS__ ):0;

#define KB(n) (1 << 10)
#define MB(n) (KB(n) << 10)
#define GB(n) (MB(n) << 10)

#define _GET_NTH_ARG(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,N,...) N
#define COUNT_ARGS(...) _GET_NTH_ARG("ignore", ##__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define min(a,b) (((a)<(b))? (a): (b))
#define max(a,b) (((a)>(b))? (a): (b))
#define argmin(a,b) (((a)<(b))? 0: 1)
#define argmax(a,b) (((a)>(b))? 0: 1)

#endif
