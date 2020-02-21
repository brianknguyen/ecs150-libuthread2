# ECS 150: Project 3 - User-level thread library (part 2)

## Implementing the Semaphore

We created a semaphore struct which has a queue to hold blocked threads and a
a count to keep track of the number of available resources.

### Sem Up and Sem Down   

When sem_down() is called, a thread attempts to grab a resource. The thread will
be able to grab a resource as long as the semaphore count is above 0.
Originally we had used a for loop to check semaphore count ensure that semaphore
count was greater than 0, but we later opted to use a while loop to ensure that
a thread goes back to sleep if a resource is no longer available by the time it
gets to run. When sem_up() is called, the blockedQueue is checked to see if
other threads are currently waiting for theresource.

### Create and Destory

When calling sem_create(count), a new semaphore will be initialized with given
count and a queue will be created. Upon calling sem_destroy(), blockedQueue's
length is checked to make sure that there are no threads currently being
blocked. If blockedQueue is empty, then the queue is destroyed and the passed
semaphore pointer is freed.

## Implementing the TPS

Our Thread Private Storage memory area was implemented by using two structs as
well as a global TPS queue.

The first struct is Page which contains the address of memory that is being
protected and a count so we are aware of how many threads are using up the
storage space.

The second struct we have is TPS which holds a pointer to Page struct as well as
thread ID. This second struct was implemented to ensure that multiple threads
can point to the same memory after cloning.

### TPS Init



### TPS Read and Write 
