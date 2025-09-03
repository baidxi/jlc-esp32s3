#pragma once

#include <stdatomic.h>

#include "types.h"

static inline int
atomic_read(const atomic_t *v)
{
    return __atomic_load_n(&v->counter, memory_order_acquire);
}

static inline int
atomic_try_cmpxchg_relaxed(atomic_t *v, int *old, int new)
{
    return __atomic_compare_exchange_n(&v->counter, old, new, 0, 
        memory_order_release, memory_order_acquire);
}

static inline void
atomic_set(atomic_t *v, int i)
{
    __atomic_store_n(&v->counter, i, memory_order_relaxed);
}

static inline void
atomic_add(int i, atomic_t *v)
{
    __atomic_fetch_add(&v->counter, i, memory_order_relaxed);
}

static inline int
atomic_add_return(int i, atomic_t *v)
{
    return __atomic_fetch_add(&v->counter, i, memory_order_release);
}

static inline int
atomic_fetch_add_relaxed(int i, atomic_t *v)
{
    return __atomic_fetch_add(&v->counter, i, memory_order_relaxed);
}

static inline int
atomic_fetch_sub_release(int i, atomic_t *v)
{
	return __atomic_fetch_sub(&v->counter, i, memory_order_release);
}