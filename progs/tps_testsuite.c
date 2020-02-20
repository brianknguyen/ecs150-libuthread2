#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <tps.h>
#include <sem.h>

int checkTPSProtection = 0;

void *latest_mmap_addr; // global variable to make address returned by mmap accessible
void *__real_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
void *__wrap_mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	latest_mmap_addr = __real_mmap(addr, len, prot, flags, fildes, off);
	return latest_mmap_addr;
}

static sem_t sem1, sem2;

char msg1[TPS_SIZE] = "This is message numero one";
char msg2[TPS_SIZE] = "This is most likely numero dos";
char msg3[TPS_SIZE] = "If this isn't message tres, I don't know what I'd do";

pthread_t tid1, tid2;

void *thread2(__attribute__((unused)) void *arg){
	char* buffer = malloc(TPS_SIZE);

	// Make sure thread can't access TPS before it has created one
	assert(tps_read(0, TPS_SIZE, buffer) == -1);
	assert(tps_write(0, TPS_SIZE, msg1) == -1);
	
	// Make sure thread 2 can't access thread 1's TPS
	if (checkTPSProtection == 2) {
		// Get TPS page address as allocated via mmap() from thread1's TPS creation
		char *tps_addr = latest_mmap_addr;
		
		// Cause an intentional TPS protection error
		tps_addr[0] = 0;
	}

	// Make sure data is copied during cloning
	memset(buffer, 0, TPS_SIZE);
	assert(tps_clone(tid1) == 0);
	assert(tps_read(0, TPS_SIZE, buffer) == 0);
	assert(!memcmp(buffer, msg1, TPS_SIZE));

	// Check that modifying the clone page doesn't modify original
	assert(tps_write(0, TPS_SIZE, msg3) == 0);

	sem_up(sem1);
	sem_down(sem2);
	return 0;
}

void *thread1(__attribute__((unused)) void *arg){

	char* buffer = malloc(TPS_SIZE);
	memset(buffer, 0, TPS_SIZE);

	// Make sure a duplicate tps can't be made
	assert(tps_create() == 0);
	assert(tps_create() == -1);

	if (checkTPSProtection == 1) {
		// Get TPS page address as allocated via mmap()
		char *tps_addr = latest_mmap_addr;
		
		// Cause an intentional TPS protection error
		tps_addr[0] = 0;
	}

	// Make sure data can be written and read
	assert(tps_write(0, TPS_SIZE, msg1) == 0);
	assert(tps_read(0, TPS_SIZE, buffer) == 0);
	assert(!memcmp(buffer, msg1, TPS_SIZE));

	// Make sure writing and reading bytes at offsets works
	memset(buffer, 0, TPS_SIZE);
	assert(tps_write(0, TPS_SIZE / 2, msg1) == 0);
	assert(tps_write(TPS_SIZE / 2, TPS_SIZE / 2, msg2) == 0);
	assert(tps_read(0, TPS_SIZE, buffer) == 0);
	assert(!memcmp(buffer, msg1, TPS_SIZE / 2));
	assert(tps_read(TPS_SIZE / 2, TPS_SIZE / 2, buffer) == 0);
	assert(!memcmp(buffer, msg2, TPS_SIZE / 2));

	// Cloning a non-existant thread's tps
	assert(tps_clone(-1) == -1);

	// Check that cloning actually clones data for another thread
	memset(buffer, 0, TPS_SIZE);
	assert(tps_write(0, TPS_SIZE, msg1) == 0);
	pthread_create(&tid2, NULL, thread2, NULL);
	sem_down(sem1);

	// Check that modifying the clone page doesn't modify original
	assert(tps_read(0, TPS_SIZE, buffer) == 0);
	assert(!memcmp(buffer, msg1, TPS_SIZE));

	// Check if you can write other data types to TPS
	int numbers[TPS_SIZE] = { 1, 2, 3, 5 };
	int* numberBuffer = malloc(TPS_SIZE);
	assert(tps_write(0, TPS_SIZE, numbers) == 0);
	assert(tps_read(0, TPS_SIZE, numberBuffer) == 0);
	assert(!memcmp(numberBuffer, numbers, TPS_SIZE));
	return 0;
}

int main(int argc, char **argv)
{
	// If argument '1' is provided, check that thread1 can't access its TPS outside of tps_read() or tps_write
	// If argument '2' is provided, check that thread2 can't access thread1's TPS
	// Expected outcome is a crash
	if (argc > 1) {
		if (!strcmp(argv[1], "1"))
			checkTPSProtection = 1;
		else if(!strcmp(argv[1], "2"))
			checkTPSProtection = 2;
	}

	sem1 = sem_create(0);
	sem2 = sem_create(0);

	/* Init TPS API */
	tps_init(1);

	/* Create thread 1 and wait */
	pthread_create(&tid1, NULL, thread1, NULL);
	pthread_join(tid1, NULL);

	/* Destroy resources and quit */
	sem_destroy(sem1);
	sem_destroy(sem2);

	printf("Finished!\n");

	return 0;
}
