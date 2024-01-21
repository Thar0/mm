#include "cclib.h"
#include <pthread.h>

ccassert(sizeof(Thread) == sizeof(pthread_t));
ccassert(sizeof(Mutex) == sizeof(pthread_mutex_t));

int thd_create(Thread* thd, void* func, void* arg) {
	return pthread_create((void*)thd, NULL, func, arg);
}

int thd_join(Thread* thd, int* ret) {
	return pthread_join(*thd, (void*)ret);
}

int mutex_init(Mutex* mutex) {
	return pthread_mutex_init((void*)mutex, NULL);
}

int mutex_destroy(Mutex* mutex) {
	return pthread_mutex_destroy((void*)mutex);
}

int mutex_lock(Mutex* mutex) {
	return pthread_mutex_lock((void*)mutex);
}

int mutex_unlock(Mutex* mutex) {
	return pthread_mutex_unlock((void*)mutex);
}
