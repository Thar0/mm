#ifndef CCLIB_THREAD_H
#define CCLIB_THREAD_H

#include "cclib-types.h"

#if __MINGW64__
typedef unsigned long long int Thread;
#else
typedef unsigned long int Thread;
#endif

#ifdef __SIZEOF_PTHREAD_MUTEX_T
typedef union {
	char     __size[__SIZEOF_PTHREAD_MUTEX_T];
	long int __align;
} Mutex;
#else
typedef intptr_t Mutex;
#endif

int thd_create(Thread*, void* func, void* arg);
int thd_join(Thread*, int* ret);
int mutex_init(Mutex*);
int mutex_destroy(Mutex*);
int mutex_lock(Mutex*);
int mutex_unlock(Mutex*);

#define mutex_scope(mutex) \
		for (int __lock = 0; !__lock; ) \
		for (int result = mutex_lock(mutex); !__lock; mutex_unlock(mutex), __lock = 1)

#endif
