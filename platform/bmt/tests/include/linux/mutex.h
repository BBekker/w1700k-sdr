/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <pthread.h>
#define DEFINE_MUTEX(name) pthread_mutex_t name = PTHREAD_MUTEX_INITIALIZER
static void (*test_before_mutex_lock)(pthread_mutex_t *);
static inline void mutex_lock(pthread_mutex_t *m)
{ if(test_before_mutex_lock) test_before_mutex_lock(m); assert(!pthread_mutex_lock(m)); }
static inline void mutex_unlock(pthread_mutex_t *m) { assert(!pthread_mutex_unlock(m)); }
