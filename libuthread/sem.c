#include <stddef.h>
#include <stdlib.h>

#include "queue.h"
#include "sem.h"
#include "thread.h"

#include <stdio.h>

typedef struct semaphore {
	size_t count;
	queue_t blockedQueue;
} semaphore;

sem_t sem_create(size_t count)
{
	sem_t newSem = malloc(sizeof(semaphore));
	if (newSem == NULL) return NULL;
	newSem->count = count;
	queue_t newQueue = queue_create();
	if (newQueue == NULL) return NULL;
	newSem->blockedQueue = newQueue;
	//printf("sem_create\n");
	return newSem;
}

int sem_destroy(sem_t sem)
{
	if (sem == NULL) return -1;
	// Can't destroy semaphore if it still contains blocked threads.
	if (queue_length(sem->blockedQueue) > 0) return -1;
	if (queue_destroy(sem->blockedQueue) < 0) return -1;
	//printf("sem_destroy\n");
	free(sem);
	return 0;
}

int sem_down(sem_t sem)
{
	enter_critical_section();
    //printf("sem_down\n");
	if (sem == NULL) return -1;
	// No resources left, so wait in queue
	if (sem->count <= 0) {
		pthread_t tid = pthread_self();
		if (queue_enqueue(sem->blockedQueue, (void*) tid) < 0) return -1;
		//printf("Blocked thread %d\n", (int) *tid);
		if (thread_block() < 0) return -1;
		//printf("sem_down: count == 0, blocked thread %d\n", (int) *tid);
	}
	// There are available resources, so decrement 
	sem->count--;
	exit_critical_section();
	return 0;
}

int sem_up(sem_t sem)
{
	enter_critical_section();
	if (sem == NULL) return -1;
	//printf("sem_up\n");
	// There are blocked threads, so unblock oldest one
	if (queue_length(sem->blockedQueue) > 0) {
		//printf("Unblocking oldest thread\n");
		pthread_t unblockedTid;
		if (queue_dequeue(sem->blockedQueue, (void**) &unblockedTid) < 0) return -1;
		if (thread_unblock(unblockedTid) < 0) return -1;
        //printf("unblocked thread %d\n", (int) *unblockedTid);
	}
	sem->count++;
	exit_critical_section();
	return 0;
}

int sem_getvalue(sem_t sem, int *sval)
{
	enter_critical_section();
	if (sem == NULL || sval == NULL) return -1;
	*sval = sem->count;
	exit_critical_section();
	return 0;
}

