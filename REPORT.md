# ECS 150: Project 3 - User-level thread library (part 2)

## Implementing the Semaphore

We created a semaphore struct which has a queue to hold blocked threads and a
count to keep track of the number of available resources.

### Sem Up and Sem Down   

When `sem_down()` is called, a thread attempts to grab a resource. If there are
there are no resources available, then the calling thread is blocked only to be
unblocked when another thread calls `sem_up()` on the semaphore. This condition
is checked in a while loop so that when the thread is unblocked, it checks if
there are any resources available (to account for the cases when another thread
has taken the resource between the time the current thread has been awoken and
scheduled).

When `sem_up()` is called, the calling thread checks if there are any threads
in the semaphore's blocked queue. If there are any, the oldest thread will be
removed from the queue and unblocked. The semaphore will then release a
resource.

### Create and Destory

When calling `sem_create(count)`, a new semaphore will be initialized with
given count and a queue will be created. Upon calling `sem_destroy()`,
`blockedQueue`'s length is checked to make sure that there are no threads
currently being blocked. If `blockedQueue` is empty, then the queue is
destroyed and the passed semaphore pointer is freed.

## Implementing the TPS

Our Thread Private Storage memory area was implemented by using two structs as
well as a global TPS list.

The TPS struct represents the Thread Private Storage for a single thread. It
holds the tid of its associated thread and a pointer to the Page struct which
is the page of memory that the thread reads and writes to.

The Page struct contains the address in memory where page of allocated memory
resides and a count that holds the number of TPSs that point to it. We thought
about keeping a list in the page struct of all the TPSs that reference the
page, but figured that the page doesn't actually need references to the
associated TPSs, rather just how many there are. The page needs to keep track
of how many TPSs are keeping track of it so that when a TPS writes to it's
page, it knows whether to make a new page or not. 

### TPS Read and Write

In `tps_read()`, we first check to see that buffer is not NULL and that the
thread has a TPS in our `tpsList`. After these checks, we use `queue_iterate()`
and our `find_TPS()` in order to find the TPS in our queue. In `tps_read()`,
the foundTPS ptr of the referenced page is then given temporary read rights.
Using `memcpy()`, we then store the proper data into the buffer.

In `tps_write()`, we start with our checks to ensure that a TPS is found and
that buffer is not `NULL`. If count is less than or equal to 1, then `memcpy()`
will be used to write the page into buffer. However, if count is greater than
1, then a new page will be created and the current page of the TPS is copied in
the new page. We then decrement the current page's count and the TPS will now
reference the newly copied page. The new page will then be written into buffer
using `memcpy()`.

### TPS Clone

In our `tps_clone()` implementation, a new TPS is created but rather than
create a new page and copy the data from the old page to the new, we just make
the new TPS reference the old page until it wants to write new data to it. The
page's count is incremented at this point.

### TPS Protection

We implemented TPS protection by only turning on read permssions when a thread
wants to read its TPS and write permissions when a thread wants to write to its
TPS. This way, the user of the library cannot access the memory of any TPS page
unless it is through `tps_read` or `tps_write`.

## TPS API Testing

We made a TPS API tester called tps_testsuite.c and it tests all the test cases
we came up with. We tested TPS by having two threads and two semaphores that
allowed the code in each thread in to be executed in a specified sequential
order. Thread 1 checks that TPS creation, reading, writing, and error cases for
each are working properly. 

Thread 1 then creates thread 2 and then calls `sem_down(sem1)` so that thread 2
can then execute. Thread 2 checks that cloning thread 1's TPS actually copies
data and that modifying the data doesn't modify thread 1's data. Thread 2 then
calls `sem_up(sem1)` and returns so that thread 1 can run. Thread 1 then
continues to check if you can write other data types to the TPS.

The test suite can also be ran with wither arguments '1' or '2'. Argument '1'
will cause thread1 to try to access its TPS without using `tps_read()` or
`tps_write()` printing out "TPS protection error!\n" to show that the library
can differentiate between TPS seg faults and normal ones. Argument '2' make
thread 2 try to access data in thread 1's data space which will cause a similar
crash.
