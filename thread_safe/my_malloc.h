#ifndef _MY_MALLOC_H_
#define _MY_MALLOC_H_
#include<stdbool.h>
#include<stdlib.h>
#include<stdio.h>
#include <unistd.h>
#include<pthread.h>

void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);

//Thread Safe malloc/free: non_locking version
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);
#endif
