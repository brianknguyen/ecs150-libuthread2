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

typedef struct Page {
	char* addr;
	int count;
} Page;

typedef struct TPS {
	Page* page;
	pthread_t tid;
} TPS;

queue_t tpsQueue = NULL;

static int find_TPS_From_Page(void *data, void* arg)
{
	TPS* a = (TPS*)data;
	void* match = arg;
	if((void*) a->page == match){
		return 1;
	}
	return 0;
}

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

static void segv_handler(int sig, siginfo_t *si, __attribute__((unused)) void *context)
{
    /*
     * Get the address corresponding to the beginning of the page where the
     * fault occurred
     */
    void* p_fault = (void*)((uintptr_t)si->si_addr & ~(TPS_SIZE - 1));

	TPS* foundTPS = NULL;
	queue_iterate(tpsQueue, find_TPS_From_Page, (void*) p_fault, (void**) &foundTPS);

    if (foundTPS != NULL)
        /* Printf the following error message */
        fprintf(stderr, "TPS protection error!\n");

    /* In any case, restore the default signal handlers */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    /* And transmit the signal again in order to cause the program to crash */
    raise(sig);
}

int tps_init(int segv)
{
	if (segv) {
		struct sigaction sa;

		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_SIGINFO;
		sa.sa_sigaction = segv_handler;
		sigaction(SIGBUS, &sa, NULL);
		sigaction(SIGSEGV, &sa, NULL);
    }
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
	void* pageAddr = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (pageAddr == (void*) -1) return -1;

	Page* newPage = malloc(sizeof(Page));
	newPage->addr = pageAddr;
	newPage->count = 1;

	TPS* newTPS = malloc(sizeof(TPS));
	newTPS->page = newPage;
	newTPS->tid = pthread_self();

	// Add new TPS to queue	
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

	mprotect(foundTPS->page->addr, length, PROT_READ);
	memcpy(buffer, foundTPS->page->addr + offset, length);
	mprotect(foundTPS->page->addr, length, PROT_NONE);
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
	if (foundTPS->page->count > 1) {
		// Allocate page for TPS
		void* pageAddr = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (pageAddr == (void*) -1) return -1;
		Page* newPage = malloc(sizeof(Page));
		newPage->addr = pageAddr;
		newPage->count = 1;
		mprotect(foundTPS->page->addr, length, PROT_WRITE | PROT_READ);
		memcpy(newPage->addr, foundTPS->page->addr, TPS_SIZE);
		foundTPS->page->count--;
		foundTPS->page = newPage;
	}
	
	mprotect(foundTPS->page->addr, length, PROT_WRITE);
	memcpy(foundTPS->page->addr + offset, buffer, length);
	mprotect(foundTPS->page->addr, length, PROT_NONE);
	exit_critical_section();
	return 0;
}

int tps_clone(pthread_t tid)
{
	// Find TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, find_TPS, (void*) tid, (void**) &foundTPS) < 0) {
		return -1;
	}
	
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}
	TPS* newTPS = malloc(sizeof(TPS));
	newTPS->page = foundTPS->page;
	newTPS->tid = pthread_self();
	newTPS->page->count++;
	queue_enqueue(tpsQueue, newTPS);
	printf("clone tid: %ld\n", newTPS->tid);
	exit_critical_section();
	return 0;
}

