#pragma once

typedef struct {
	int counter;
} atomic_t;

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)