### Project description
Project 2A is based upon the concept of data races and synchronization that occur when multiple threads are executing the same process. Since threads share the same data segment and code segment, when function calls are performed by different threads and accessing the same values in memory, they may be doing so out of sync and performing read and write operations for example, out of order. To display such behavior, this project consists of 2 parts: first showing the possibility of a data race by updating a shared variable, then exhibiting a data race with a more complex data structure such as a doubly linked list, both fixed through synchronization methods. This project is packaged into a tarball and includes 3 C source files, an interface file a README file, a Makefile, 2 csv files with the results of comparing multiple features of data races and finally graphs to visually display these results.

The first part of this project consists of using the pthread library to create and launch multiple threads that will update a shared variable by incrementing and decrementing it by 1. As more threads are working simultaneously, these add operations will be performed out of order and due to a data race, the variable will be updated to a non-zero value. To exacerbate race conditions, threads are forced to yield their CPU time during critical sections, amplifying the unsynchronization of this operation. In order to fix this behavior, 3 different synchronization methods are exploited. The first is using a mutex lock which is a globally-allocated lock that exemplifies mutually exclusive object behavior. The second is a spin lock implementation that continuously attempts to get the lock when it if being used by another thread, and therefore keeps "spinning" until has acquired it. The final synchronization method is the use of an atomic operation to ensure that high level instructions such as adding are actually performed as one all-encompassing instruction, through the use of compare and swap.

The second part of this project emulates the first part except it uses the insertion and deletion of elements in a list to exemplify data races. A doubly linked list is created with random values and the list length is checked after all threads have returned to verify that it is zero. In the case of a data race, this non-zero length is then fixed through a mutex lock or a spin lock as in part 1. Since updating a list requires more instructions than an odd operation, yielding is performed at multiple instances including the insertion of an element in the list, the deletion of an element, and the lookup of the element in the list.

Multiple tests were performed to observe the behavior of data races in these two scenarios and the results were analyzed for the following patterns:

### Project Analysis
###### QUESTION 2.1.1 - causing conflicts:
After running the program for a range of iterations, it takes about 1000 iterations to consistently result in a non zero sum although the values at 1000 iterations is still close to 0, therefore to ensure a failure, iterations greater than 1000 should be used.

Why does it take many iterations before errors are seen?
Why does a significantly smaller number of iterations so seldom fail?
It takes many iterations to detect errors because in order for an error to occur, there must be a context switch and the add operation must be interrupted in the middle. When the iterations are low, there is a very low chance that the add() function will not terminate before another thread takes over.  In order to observea data race, the amount of time it takes to run this certain low number of iterations is still lower than the amount of time that is required for a new
thread to take over, therefore explaining why a signficantly smaller number of iterations so seldom fails.

###### QUESTION 2.1.2 - cost of yielding:
When the yield option is added, the average time per operation extensively increases.

Why are the --yield runs so much slower? Where is the additional time going?
The --yield runs are much slower due to the overhead that is inquired with context switching between the multiple threads being run simultaneously. Calling the sched_yield() functions requires an amount equal to 2 times the number of threads times number of iterations for context switches in addition to the context switches between each running thread when they acquire CPU time, accounting for this additional time

Is it possible to get valid per-operation timings if we are using the --yield option? If so, explain how. If not, explain why not.
It is not possible to get valid per-operation timings with the --yield option because we as the user are not aware of time needed to perform these context switches and therefore cannot calculate this value.

###### QUESTION 2.1.3 - measurement errors:
As the number of iterations for the same number of threads increases, the average time per operation decreases.

Why does the average cost per operation drop with increasing iterations?
With more iterations performed per thread, the amount of time needed by the main process to create these threads is diluted by the growing number of iterations performed by each thread. Therefore the average cost per operation decreases as the numerator is fixed but the denominator increases.

If the cost per iteration is a function of the number of iterations, how do we know how many iterations to run (or what the "correct" cost is)?
In order to determine the more or less the number of iterations to run, essentially the "correct" cost, we can continuously increase the number of iterations until the cost per operation converges as the cost of context switching becomes so small it is irrelevant.

###### QUESTION 2.1.4 - costs of serialization:
When adding synchronization options, all three methods fix the data race conditions and return a sum of zero. The swap and compare atomic add with yield incurs the highest cost per operation yet when the yield option is not chosen, the spin lock incurs a higher cost for the average time per operation.

Why do all of the options perform similarly for low numbers of threads?
For low number of threads, all the synchronization options perform similarly because similarly for the low number of threads not incurring much overhead cost due to context switching, the added overhead of the synchronization implementation is negligible since is not applied often.

Why do the three protected operations slow down as the number of threads rises?
As the number of threads rises, the waiting time often accompanying the synchronization method increases as well since more threads are trying to access shared data that is not available.

For part 2 of the project, it takes about [] to consistently demonstrate a problem which is exemplified through a list length that is not 0. When the yield option is added, 

###### QUESTION 2.2.1 - scalability of Mutex
Compare the variation in time per mutex-protected operation vs the number of threads in Part-1 (adds) and Part-2 (sorted lists).
Comment on the general shapes of the curves, and explain why they have this shape.
Comment on the relative rates of increase and differences in the shapes of the curves, and offer an explanation for these differences.
For the add operation protected by the mutex synchronization method, the trend displays a plateau which suggests that the add operation is minimal enough that it is not really affecting the time per operation for the amount of threads run simultaneously. When running this experiment multiple times, the trend shows a slight increase in the cost per operation as the number of threads goes up. On the other hand, the sorted list protected by the mutex synchronization shows the same upward trend yet the slope of the increase is steeper as the cost of locking and unlocking the critical section to operate on a more complex data structure such as a doubly linked list makes the average time per operation increase at a higher rate.The general shapes of the curves is a linear with a positive slope (linear increase) due to a linear increase in the time per operation as the number of threads increases and the cost of adding a protection mechanism increases in similar fashion.

###### QUESTION 2.2.2 - scalability of spin locks
Compare the variation in time per protected operation vs the number of threads for list operations protected by Mutex vs Spin locks. Comment on the general shapes of the curves, and explain why they have this shape.
Comment on the relative rates of increase and differences in the shapes of the curves, and offer an explanation for these differences.
As the number of threads increases to perform list operations protected by mutex and spin locks, the time per protected operation increases as well, showing an upward slope. This is again due to the increase in time each added thread must wait before it can reach the critical section to perform the list operation. The rate of increase of the list operations protected by spin locks is higher than those protected by mutexes as spin locks are consistently asking for the locked critical section (ie spinning) and therefore taking up CPU time which makes the completion of each oepration much slower as the CPU time is "wasted" by continuously asking for the locks.
