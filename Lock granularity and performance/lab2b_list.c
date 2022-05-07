#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "SortedList.h"

int threads;
int iterations;

int opt_yield;
char opt_yield_name[10] = "";

char sync_opt = 'n';

pthread_mutex_t mutex;

SortedList_t* listhead;
SortedListElement_t* pool;

long lock=0;

unsigned long *lock_waittime;

//char alphabet[30] = "abcdefghijklmnopqrstuvwxyz";

void handle_signal(){
	fprintf(stderr, "Segmentation fault occured and was caught by signal handler \n");
	exit(2);
}

//this is taken from Discussion 1B slides
static inline unsigned long get_nanosec_from_timespec(struct timespec *spec) {
	unsigned long ret = spec -> tv_sec; //seconds
	ret = ret* 1000000000 + spec -> tv_nsec; //nanoseconds
	return ret;
}

void* thread_worker(void* ids) {
	int i;
	int startIndex = *((int*) ids);
	struct timespec start_time, end_time;

	for (i = startIndex; i < startIndex + iterations; i++) {
		//fprintf(stdout, "%d\n",i);
		if (sync_opt == 'n') {
			SortedList_insert(listhead, &pool[i]); //insert element
		}
		else if (sync_opt == 'm') {
			//record waiting time to acquire lock
			clock_gettime(CLOCK_MONOTONIC,&start_time);
			pthread_mutex_lock(&mutex); //acquire
			clock_gettime(CLOCK_MONOTONIC,&end_time);

			SortedList_insert(listhead, &pool[i]); //insert element
			pthread_mutex_unlock(&mutex); //free the lock

			lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
		}
		else if (sync_opt == 's') {
			//record waiting time to acquire lock
			clock_gettime(CLOCK_MONOTONIC,&start_time);
			while (__sync_lock_test_and_set(&lock,1));
			clock_gettime(CLOCK_MONOTONIC,&end_time);
			lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);

			SortedList_insert(listhead, &pool[i]); //insert element
			__sync_lock_release(&lock);
		}
	}

	int length_list = 0;
	if (sync_opt == 'n') {
		length_list = SortedList_length(listhead); //check length
	}
	else if (sync_opt == 'm') {
		clock_gettime(CLOCK_MONOTONIC,&start_time);
		pthread_mutex_lock(&mutex); //acquire
		clock_gettime(CLOCK_MONOTONIC,&end_time);

		length_list = SortedList_length(listhead); //check length
		pthread_mutex_unlock(&mutex); //free the lock

		lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);
	}
	else if (sync_opt == 's') {
		clock_gettime(CLOCK_MONOTONIC,&start_time);
		while (__sync_lock_test_and_set(&lock,1));
		clock_gettime(CLOCK_MONOTONIC,&end_time);

		lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);

		length_list = SortedList_length(listhead); //check length
		__sync_lock_release(&lock);
	}
	if (length_list < 0) {
		fprintf(stderr, "Corrupted list, list length less than zero\n");
		exit(1);
	}

	SortedListElement_t* e;

	for (i = startIndex; i < startIndex + iterations; i++) {
		if (sync_opt == 'n') {
			e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
			SortedList_delete(e); //remove inserted element
		}
		else if (sync_opt == 'm') {
			clock_gettime(CLOCK_MONOTONIC,&start_time);
			pthread_mutex_lock(&mutex); //acquire
			clock_gettime(CLOCK_MONOTONIC,&end_time);

			lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);

			e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
			SortedList_delete(e); //remove inserted element
			pthread_mutex_unlock(&mutex); //free the lock
		}
		else if (sync_opt == 's') {
			clock_gettime(CLOCK_MONOTONIC,&start_time);
			while (__sync_lock_test_and_set(&lock,1));
			clock_gettime(CLOCK_MONOTONIC,&end_time);

			lock_waittime[startIndex/iterations] += get_nanosec_from_timespec(&end_time) - get_nanosec_from_timespec(&start_time);

			e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
			SortedList_delete(e); //remove inserted element
			__sync_lock_release(&lock);
		}
	}
	//if corrupted list, log error message to stderr and exit immediately without producing results record
	return NULL;
}

int main (int argc, char **argv) {
	/*Process all arguments and store results in variables*/
	struct option optionargs[] = {
		{"threads", 1, NULL, 't'},
		{"iterations", 1, NULL, 'i'},
		{"yield",1, NULL, 'y'},
		{"sync", 1, NULL, 's'},
		{"lists", 1, NULL, 'l'},
		{0,0,0,0}
	};
	//fprintf(stdout, "entered main");
	
	char cmd_option;

	while(1) {
		cmd_option = getopt_long(argc, argv, "", optionargs, NULL);
		if (cmd_option < 0) {
			break;
		}
		else if (cmd_option == 't') {
			threads = atoi(optarg);
		}
		else if (cmd_option == 'i') {
			iterations = atoi(optarg);
		}
		else if (cmd_option == 'y') {
			int y_len = strlen(optarg);
			if (y_len > 3 || y_len == 0){
				fprintf(stderr, "Incorrect argument length for --yield\n");
				exit(1);
			}
			int i;
			for (i=0; i < y_len; i++) {
				if (optarg[i] == 'i') {
					opt_yield |= INSERT_YIELD;
					strcat(opt_yield_name, "i"); //add to opt_yield_name
				}
				else if (optarg[i] == 'd') {
					opt_yield |= DELETE_YIELD;
					strcat(opt_yield_name, "d");
				}
				else if (optarg[i] == 'l') {
					opt_yield |= LOOKUP_YIELD;
					strcat(opt_yield_name, "l");
				}
				else {
					fprintf(stderr, "Incorrect argument for --yield\n");
					exit(1);
				}
			}
		}
		else if (cmd_option == 's') {
			sync_opt = *optarg;
			if(sync_opt == 'm') {
				pthread_mutex_init(&mutex, NULL);
			}
		}
		else if (cmd_option == '?'){
			fprintf(stderr, "Incorrect arguments. Correct usage: ./lab2_list --threads# --iterations=# --yield='idl' --sync='m' or 's'\n");
			exit(1);
		}
	}
	//fprintf(stdout, "Options parsed\n");

	signal(SIGPIPE, handle_signal);

	//initialize an empty list
	listhead = malloc(sizeof(SortedList_t));
	listhead->key = NULL;
	listhead->next = listhead->prev = listhead; //circular list

	//create and initialize with random keys required number of threads*iteration of list elements
	pool = malloc(threads*iterations*sizeof(SortedListElement_t));

	//fprintf(stdout, "Initializations finished\n");

	srand(time(NULL));
	int n;
	for (n = 0; n < threads*iterations; n++) {
		int rand_idx = (rand() % 26) +'a';
		char* randkey = malloc(2*sizeof(char));
		
		randkey[0] = rand_idx;
		randkey[1] = '\0';

		pool[n].key = randkey;
	}
	//fprintf(stdout, "Random keys initialized\n");

	struct timespec begin, end;
    unsigned long totalrun = 0;

    //note starting time
	clock_gettime(CLOCK_MONOTONIC, &begin); //record start time

	//start specified number of threads
	pthread_t *thread_arr;
	thread_arr = malloc(threads * sizeof(pthread_t));

	//fprintf(stdout, "Threads initialized\n");

	//store ids of threads initialized
	int *thread_ids = malloc(threads*sizeof(int));

	//initialize array to store lock wait times
	lock_waittime = malloc(threads*sizeof(unsigned long));

	int i;
	for (i=0; i < threads; i++) {
		thread_ids[i] = i*iterations;
		if (pthread_create(&thread_arr[i], NULL, thread_worker, &thread_ids[i]) != 0) {
			fprintf(stderr, "Creating threads failed. Error: %s\n", strerror(errno));
			exit(1);
		} //create a new thread
	}

	//waits for all threads to complete
	for (i=0; i < threads; i++) {
		if (pthread_join(thread_arr[i], NULL) != 0) {
			fprintf(stderr, "Joining threads after they terminated failed. Error: %s\n", strerror(errno));
			exit(1);
		} //join waits for thread to finish before main thread continues
	}

	//note end time for the run
	clock_gettime(CLOCK_MONOTONIC, &end); 

	//check length of list to confirm it is zero
	if (SortedList_length(listhead) != 0) {
		fprintf(stderr, "Corrupted list, list length less than zero\n");
		exit(2);
	}

	totalrun = get_nanosec_from_timespec(&end) - get_nanosec_from_timespec(&begin);

	long long totalops = threads*iterations*3;
	long long avetime = totalrun/totalops;

	//establish test name
	char* testname = "";
	if (sync_opt == 'n') {
		if (strlen(opt_yield_name) == 0) { //no yield option
			testname = "none-none";
		}
		else {
			testname = strcat(opt_yield_name,"-none");
		}
	}
	else if (sync_opt == 'm') {
		if (strlen(opt_yield_name) == 0) {
			testname = "none-m";
		}
		else {
			testname = strcat(opt_yield_name,"-m");
		}
	}
	else if (sync_opt == 's') {
		if (strlen(opt_yield_name) == 0) {
			testname = "none-s";
		}
		else {
			testname = strcat(opt_yield_name,"-s");
		}
	}
	

	//print_out(name of test, threads, iterations, list number, total_operations, total_runtime, average_time);
	fprintf(stdout,"list-%s,%d,%d,1,%lld,%lu,%lld\n", testname, threads, iterations, totalops, totalrun, avetime);
	//name of test: add-none for most basic usage, add-yield for yield option
	//total number of operations: threads * iterations * 3
	//total runtime in nanoseconds
	//average time per operation in nanoseconds
	//total at end of run (0 if no conflicting updates)

	//free the allocated memory
	free(pool);
	free(listhead);
	free(thread_ids);
	free(thread_arr);

	//exit without error
	exit(0);
}