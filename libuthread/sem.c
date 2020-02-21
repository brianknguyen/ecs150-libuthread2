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
	return newSem;
}

int sem_destroy(sem_t sem)
{
	if (sem == NULL) return -1;
	// Can't destroy semaphore if it still contains blocked threads.
	if (queue_length(sem->blockedQueue) > 0) return -1;
	if (queue_destroy(sem->blockedQueue) < 0) return -1;
	free(sem);
	return 0;
}

int sem_down(sem_t sem)
{
	enter_critical_section();
	if (sem == NULL) return -1;
	// No resources left, so wait in queue
	// Keep checking whether the sem count is 0 because another thread could interrupt and steal the resource before this thread is scheduled
	while (sem->count <= 0) {
		pthread_t tid = pthread_self();
		if (queue_enqueue(sem->blockedQueue, (void*) tid) < 0) return -1;
		if (thread_block() < 0) return -1;
	}
	
	sem->count--;
	exit_critical_section();
	return 0;
}

int sem_up(sem_t sem)
{
	enter_critical_section();
	if (sem == NULL) return -1;
	// There are blocked threads, so unblock oldest one
	if (queue_length(sem->blockedQueue) > 0) {
		pthread_t unblockedTid;
		if (queue_dequeue(sem->blockedQueue, (void**) &unblockedTid) < 0) {
			exit_critical_section();
			return -1;
		}
		if (thread_unblock(unblockedTid) < 0) {
			exit_critical_section();
			return -1;
		}
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

