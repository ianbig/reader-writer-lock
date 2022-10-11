#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "rwlock.h"

/* rwl implements a reader-writer lock.
 * A reader-write lock can be acquired in two different modes, 
 * the "read" (also referred to as "shared") mode,
 * and the "write" (also referred to as "exclusive") mode.
 * Many threads can grab the lock in the "read" mode.  
 * By contrast, if one thread has acquired the lock in "write" mode, no other 
 * threads can acquire the lock in either "read" or "write" mode.
 */

// plan
// 1. reader - writer lock with no priority
// just one cond and w_active[0], w_waiting for write count
// 2. add priority

/**
 * @param rwl - lock metadata
 * @return int - the number of active writer
 * **/
int get_active_writer_count(rwl * l) {
	if (l->w_active[0] > 0) {
		assert(l->w_active[0] == 1);
		return 1;
	} else if (l->w_active[1] > 0) {
		assert(l->w_active[1] == 1);
		return 1;
	} else if (l->w_active[2] > 0) {
		assert(l->w_active[2] == 1);
		return 1;
	}

	return 0;
}

/**
 * @param rwl - lock metadata
 * @return int - the index of highest priority of current waiting thread
 * **/
int get_highest_waiting_writer_priority(rwl * l) {
	if (l->w_wait[0] > 0) {
		return 0;
	} else if (l->w_wait[1] > 0) {
		return 1;
	} else if (l->w_wait[2] > 0) {
		return 2;
	}
	return -1;
}

//rwl_init initializes the reader-writer lock 
void
rwl_init(rwl *l)
{
	// initialization of read/write lock
	int rc = pthread_mutex_init(&l->mutex, NULL);
	assert(rc == 0);
	rc = pthread_cond_init(&l->r_cond, NULL);
	assert(rc == 0);
	l->r_active = 0;
	l->r_wait = 0;

	for (size_t i = 0; i < 3; i++) {
		rc = pthread_cond_init(&l->w_cond[i], NULL);
		assert(rc == 0);
		l->w_active[i] = 0;
		l->w_wait[i] = 0;
	}
}



//rwl_rlock attempts to grab the lock in "read" mode
void
rwl_rlock(rwl *l)
{
	pthread_mutex_lock(&l->mutex);
	l->r_wait++;
	while ((get_active_writer_count(l)) != 0) {
		pthread_cond_wait(&l->r_cond, &l->mutex);
	}
	l->r_wait--;

	l->r_wait++;
	while (get_highest_waiting_writer_priority(l) != -1) {
		pthread_cond_wait(&l->r_cond, &l->mutex);
	}
	l->r_wait--;

	l->r_active++;
	pthread_mutex_unlock(&l->mutex);
}


//rwl_runlock unlocks the lock held in the "read" mode
void
rwl_runlock(rwl *l)
{
	int index = 0;
	pthread_mutex_lock(&l->mutex);
	l->r_active--;
	if (l->r_active == 0) {
		pthread_cond_broadcast(&l->r_cond);
	}
	pthread_mutex_unlock(&l->mutex);
}


//rwl_wlock attempts to grab the lock in "write" mode
void
rwl_wlock(rwl *l, int priority)
{
	pthread_mutex_lock(&l->mutex);

	l->w_wait[priority]++;
	while (l->r_active > 0) {
		pthread_cond_wait(&l->r_cond, &l->mutex);
	}
	while ((get_active_writer_count(l)) != 0) {
		pthread_cond_wait(&l->w_cond[priority], &l->mutex);
	}
	while (priority > 0 && l->w_wait[0] > 0) {
		pthread_cond_wait(&l->w_cond[priority], &l->mutex);
	}
	while (priority > 1 && l->w_wait[1] > 0) {
		pthread_cond_wait(&l->w_cond[priority], &l->mutex);
	}
	l->w_wait[priority]--;

	l->w_active[priority]++;
	pthread_mutex_unlock(&l->mutex);	
}

//rwl_wunlock unlocks the lock held in the "write" mode
void
rwl_wunlock(rwl *l, int priority)
{
	pthread_mutex_lock(&l->mutex);
	l->w_active[priority]--;
	
	int waiting_writer = get_highest_waiting_writer_priority(l);
	if (waiting_writer != -1) {
		pthread_cond_broadcast(&l->w_cond[waiting_writer]);
	} else {
		pthread_cond_broadcast(&l->r_cond);
	}

	assert(l->r_active == 0);
	pthread_mutex_unlock(&l->mutex);
}