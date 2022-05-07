#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
//need to add APIs for functions included

int threads;
int iterations;
int opt_yield;
char sync_opt = 'n';

long long sum = 0;
long lock = 0;

pthread_mutex_t mutex;

static inline unsigned long get_nanosec_from_timespec(struct timespec *spec) {
	unsigned long ret = spec -> tv_sec; //seconds
	ret = ret* 1000000000 + spec -> tv_nsec; //nanoseconds
	return ret;
}

void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield) {
		sched_yield();
	}
	*pointer = sum;
}

void atomic_add(long long *pointer, long long value) {
	long long prev, new;
	for(;;) {
		prev = *pointer;
		new = prev + value;
		if (opt_yield) {
			sched_yield();
		}
		if(__sync_val_compare_and_swap(pointer, prev, new) == prev) {
			break;
		}
	//}while(__sync_bool_compare_and_swap(pointer, prev, sum) == false);
	}
}

//this is for regular add, first part of Part A
void* thread_worker() {
	int i;
	if (sync_opt == 'n') {
		for (i = 0; i < iterations; i++) {
			add(&sum, 1);
		}
		for (i = 0; i < iterations; i++) {
			add(&sum, -1);
		}
	}
	else if (sync_opt == 'm') { // this is for the mutex implementation
		pthread_mutex_lock(&mutex); //acquire
		for (i = 0; i < iterations; i++) {
			add(&sum, 1);
		}
		for (i = 0; i < iterations; i++) {
			add(&sum, -1);
		}
		pthread_mutex_unlock(&mutex); //free the lock
	}
	else if (sync_opt == 's') { //this is for the spinlock implementation
		while (__sync_lock_test_and_set(&lock,1));
		for (i = 0; i < iterations; i++) {
			add(&sum, 1);
		}
		for (i = 0; i < iterations; i++) {
			add(&sum, -1);
		}
		__sync_lock_release(&lock);
	}
	else if (sync_opt =='c') { //this is for atomic add
		// bool sync_bool_compare_and_swap(long long *p, long long old, long long new) {
		// 	if (*p == old) { //see if value has been changed
		// 		*p = new; //if not, set it to new value
		// 		return true; //tell caller he succeeded
		// 	}
		// 	else { //someone else changed *p
		// 		return false; //tell caller he failed
		// 	}
		// }
		for (i = 0; i < iterations; i++) {
			atomic_add(&sum, 1);
		}
		for (i = 0; i < iterations; i++) {
			atomic_add(&sum, -1);
		}
	}
	//must add this or non void function return
	return NULL;
}



int main (int argc, char **argv) {
	/*Process all arguments and store results in variables*/
	struct option optionargs[] = {
		{"threads", 1, NULL, 't'},
		{"iterations", 1, NULL, 'i'},
		{"yield", 0, NULL, 'y'},
		{"sync", 1, NULL, 's'},
		{0,0,0,0}
	};

	char cmd_option;

	while (1) {
    	cmd_option = getopt_long(argc, argv, "", optionargs, NULL);
    	if (cmd_option < 0) {
      		break;
    	}
    	else if (cmd_option == 't') {
      		threads = atoi(optarg);
    	}
    	else if (cmd_option =='i') {
    		iterations = atoi(optarg);
    	}
    	else if (cmd_option == 'y') {
    		opt_yield = 1;
    	}
    	else if (cmd_option == 's') {
    		sync_opt = *optarg; //not sure why we need to dereference pointer
    		if (sync_opt == 'm') {
    			pthread_mutex_init(&mutex, NULL);
    		}
    	}
    	else if (cmd_option == '?') {
      		fprintf(stderr, "Incorrect argument. Correct usage: lab2_add --threads=# --iterations=#\n");
      		exit(1);
    	}
    }

    struct timespec begin, end;
    unsigned long totalrun = 0;

	clock_gettime(CLOCK_MONOTONIC, &begin); //record start time

	pthread_t *thread_arr;

	thread_arr = malloc(threads * sizeof(pthread_t));

	int i;

	for (i=0; i < threads; i++) {
		if (pthread_create(&thread_arr[i], NULL, thread_worker, NULL) != 0) {  //can't have iterations as argument because throws incompatible warning
			fprintf(stderr, "Creating threads failed. Error: %s\n", strerror(errno));
			exit(1);
		} //create a new thread
	}
	for (i=0; i < threads; i++) {
		if (pthread_join(thread_arr[i], NULL) != 0) {
			fprintf(stderr, "Joining threads after they terminated failed. Error: %s\n", strerror(errno));
			exit(1);
		} //join waits for thread to finish before main thread continues
	}

	clock_gettime(CLOCK_MONOTONIC, &end); //record end time

	totalrun = get_nanosec_from_timespec(&end) - get_nanosec_from_timespec(&begin);

	long long totalops = threads*iterations*2;
	long long avetime = totalrun/totalops;

	//establish test name
	char* testname = "";
	if (sync_opt == 'n') {
		if (opt_yield) {
			testname = "yield-none";
		}
		else {
			testname = "none";
		}
	}
	else if (sync_opt == 'm') {
		if (opt_yield) {
			testname = "yield-m";
		}
		else {
			testname = "m";
		}
	}
	else if (sync_opt == 's') {
		if (opt_yield) {
			testname = "yield-s";
		}
		else {
			testname = "s";
		}
	}
	else if (sync_opt == 'c') {
		if (opt_yield) {
			testname = "yield-c";
		}
		else {
			testname = "c";
		}
	}
	
	//print_out(name of test, threads, iterations, total_operations, total_runtime, average_time, sum);
	fprintf(stdout,"add-%s,%d,%d,%lld,%lu,%lld,%lld\n", testname, threads, iterations, totalops, totalrun, avetime, sum);
	//name of test: add-none for most basic usage, add-yield for yield option
	//total number of operations: threads * iterations * 2
	//total runtime in nanoseconds
	//average time per operation in nanoseconds
	//total at end of run (0 if no conflicting updates)

	//free the allocated memory
	free(thread_arr);
	//exit without error
	exit(0);
}
