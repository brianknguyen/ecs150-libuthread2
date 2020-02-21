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


typedef struct Page {
	char* addr;
	int count;
} Page;

typedef struct TPS {
	Page* page;
	pthread_t tid;
} TPS;

queue_t tpsQueue = NULL;

// Find TPS based on its page's starting address
static int findTpsFromPageAddr(void *data, void* arg)
{
	TPS* a = (TPS*)data;
	void* match = arg;
	if((void*) a->page->addr == match){
		return 1;
	}
	return 0;
}

// Find TPS based on tid
static int findTpsFromTid(void *data, void *arg)
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
	queue_iterate(tpsQueue, findTpsFromPageAddr, (void*) p_fault, (void**) &foundTPS);

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

	// Attempt to find thread's current TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}
	// If the current thread already has a TPS, error
	if (foundTPS != NULL) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();

	// Allocate and initialize page for TPS
	void* pageAddr = mmap(NULL, TPS_SIZE, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (pageAddr == (void*) -1) return -1;

	Page* newPage = malloc(sizeof(Page));
	newPage->addr = pageAddr;
	newPage->count = 1;

	// Allocate and initialize new TPS
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
	// Find the thread's TPS
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}

	// If the thread has no TPS, error
	if(foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}

	// If the thread's TPS is not the only one using the page it refers to, the just decrement the count on the page
	if (foundTPS->page->count > 1) {
		foundTPS->page->count--;
	}
	// If the thread's TPS is the only one using the page it refers to, then we can delete the page
	else {
		munmap(foundTPS->page->addr, TPS_SIZE);
		free(foundTPS->page);
		free(foundTPS);
	}

	// Delete the TPS either way
	queue_delete(tpsQueue, foundTPS);
	exit_critical_section();

	return 0;
}

int tps_read(size_t offset, size_t length, void *buffer)
{
	// Buffer can't be NULL
	if (buffer == NULL) return -1;

	// Find thread's TPS to read from
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}

	// If no TPS is found, error
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}

	// Allow reading, read, then disable reading
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
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) pthread_self(), (void**) &foundTPS) < 0) {
		exit_critical_section();
		return -1;
	}

	// If no TPS is found, error
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}
	
	// If there are multiple TPSs that are using the page this thread's TPS is associated with,
	// we have to make a new page, copy the data from the old page to the new one and then decrement
	// the count of the old page
	if (foundTPS->page->count > 1) {
		// Allocate page for TPS
		void* pageAddr = mmap(NULL, TPS_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (pageAddr == (void*) -1) return -1;
		Page* newPage = malloc(sizeof(Page));
		newPage->addr = pageAddr;
		newPage->count = 1;
		// Allow reading and writing
		mprotect(foundTPS->page->addr, length, PROT_WRITE | PROT_READ);
		// Copy data from old page to the new one
		memcpy(newPage->addr, foundTPS->page->addr, TPS_SIZE);
		foundTPS->page->count--;
		foundTPS->page = newPage;
	}
	
	// Write new data to the new page
	mprotect(foundTPS->page->addr, length, PROT_WRITE);
	memcpy(foundTPS->page->addr + offset, buffer, length);
	mprotect(foundTPS->page->addr, length, PROT_NONE);
	exit_critical_section();
	return 0;
}

int tps_clone(pthread_t tid)
{

	// Attempt to find thread's current TPS
	TPS* curTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) pthread_self(), (void**) &curTPS) < 0) {
		exit_critical_section();
		return -1;
	}
	// If the current thread already has a TPS, error
	if (curTPS != NULL) {
		exit_critical_section();
		return -1;
	}
	exit_critical_section();

	// Find TPS with given tid
	TPS* foundTPS = NULL;
	enter_critical_section();
	if (queue_iterate(tpsQueue, findTpsFromTid, (void*) tid, (void**) &foundTPS) < 0) {
		return -1;
	}
	
	// If the TPS doesn't exist, error
	if (foundTPS == NULL) {
		exit_critical_section();
		return -1;
	}

	// Allocate a new TPS whose page is the same one as the TPS with the given tid
	// and increment the page's count
	TPS* newTPS = malloc(sizeof(TPS));
	newTPS->page = foundTPS->page;
	newTPS->tid = pthread_self();
	newTPS->page->count++;
	queue_enqueue(tpsQueue, newTPS);
	exit_critical_section();
	return 0;
}

