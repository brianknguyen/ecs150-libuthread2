#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "queue.h"
#include "thread.h"
#include "tps.h"

/* TODO: Phase 2 */

typedef struct TPS {
	char* page;
	pthread_t tid;
} TPS;

queue_t tpsQueue = NULL;

// Find TPS based on tid
static int find_TPS(void *data, void *arg)
{
    TPS *a = (TPS*)data;
    int match = (int)(long) arg;
    if ((int)(long) a->tid == match){
        return 1;
    }
    return 0;
}

int tps_init(int segv)
{
	int hi = segv;
	hi++;
	return 0;
}

int tps_create(void)
{
	enter_critical_section();
	// Initialize tpsQueue if it hasn't been already
	if(tpsQueue == NULL) {
		tpsQueue = queue_create();
		if (tpsQueue == NULL) {
			exit_critical_section();
			return -1;
		}
	}
	exit_critical_section();

	// If the current thread already has a TPS, error
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, find_TPS, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}
	if (foundTPS != NULL) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();

	// Allocate page for TPS
	void* page = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (page == (void*) -1) return -1;

	// Add new TPS to queue	
	TPS* newTPS = malloc(sizeof(TPS));
	newTPS->page = page;
	newTPS->tid = pthread_self();

	enter_critical_section();
	if (queue_enqueue(tpsQueue, newTPS) < 0) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();

	return 0;
}

int tps_destroy(void)
{

	return 0;
}

int tps_read(size_t offset, size_t length, void *buffer)
{
	// Buffer can't be NULL
	if (buffer == NULL) return -1;

	// Find TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, find_TPS, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}

	// If no TPS is found, error
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}
	memcpy(buffer, foundTPS->page + offset, length);
	exit_critical_section();

	return 0;
}

int tps_write(size_t offset, size_t length, void *buffer)
{
	if (buffer == NULL) return -1;

	// Find TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, find_TPS, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}

	// If no TPS is found, error
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}
	memcpy(foundTPS->page + offset, buffer, length);
	exit_critical_section();
	return 0;
}

int tps_clone(pthread_t tid)
{
	void* page = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (page == (void*) -1) return -1;
	
	// Find TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, find_TPS, (void*) tid, (void**) &foundTPS) < 0) {
		return -1;
	}
	
	memcpy(page, foundTPS->page, TPS_SIZE);
	TPS* newTPS = malloc(sizeof(TPS));
	newTPS->page = page;
	newTPS->tid = pthread_self();

	queue_enqueue(tpsQueue, newTPS);

	return 0;
}

