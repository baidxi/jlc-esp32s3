#pragma once

#include <stdio.h>
#include <stddef.h>

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))

#ifndef static_assert

/* 兼容标准C的static_assert实现 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define static_assert(expr, msg) _Static_assert(expr, msg)
#else
    #define static_assert(expr, msg) typedef char __static_assert_##__line__[(expr) ? 1 : -1]
#endif
#endif