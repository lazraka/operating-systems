#include "SortedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

//implement all four methods in SortedList.c
//identifies critical section in each of four methods and calls pthread_yield or sched_yield

/**
 * SortedList (and SortedListElement)
 *
 *	A doubly linked list, kept sorted by a specified key.
 *	This structure is used for a list head, and each element
 *	of the list begins with this structure.
 *
 *	The list head is in the list, and an empty list contains
 *	only a list head.  The next pointer in the head points at
 *      the first (lowest valued) elment in the list.  The prev
 *      pointer in the list head points at the last (highest valued)
 *      element in the list.  Thus, the list is circular.
 *
 *      The list head is also recognizable by its NULL key pointer.
 *
 * NOTE: This header file is an interface specification, and you
 *       are not allowed to make any changes to it.
 */
// struct SortedListElement {
// 	struct SortedListElement *prev;
// 	struct SortedListElement *next;
// 	const char *key;
// };
// typedef struct SortedListElement SortedList_t;
// typedef struct SortedListElement SortedListElement_t;


/**
 * SortedList_insert ... insert an element into a sorted list
 *
 *	The specified element will be inserted in to
 *	the specified list, which will be kept sorted
 *	in ascending order based on associated keys
 *
 * @param SortedList_t *list ... header for the list
 * @param SortedListElement_t *element ... element to be added to the list
 */
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
	//if list is empty, then fail
	if (list == NULL || element == NULL) {
		return;
	}

	SortedListElement_t* curr_elem = list->next;

	//while (curr_elem!= list && strcmp(element->key, curr_elem->key) > 0) {
	while (curr_elem!= list) {
		if (strcmp(element->key, curr_elem->key) > 0) {
			curr_elem = curr_elem->next;
		}
		else {
			break;
		}
	}
	if (opt_yield & INSERT_YIELD) { //constant operand so use & not &&
		sched_yield();
	}

	element->prev = curr_elem->prev;
	element->next = curr_elem ;

	curr_elem->prev->next = element;
	curr_elem->prev = element;
	
}

/**
 * SortedList_delete ... remove an element from a sorted list
 *
 *	The specified element will be removed from whatever
 *	list it is currently in.
 *
 *	Before doing the deletion, we check to make sure that
 *	next->prev and prev->next both point to this node
 *
 * @param SortedListElement_t *element ... element to be removed
 *
 * @return 0: element deleted successfully, 1: corrtuped prev/next pointers
 *
 */
int SortedList_delete( SortedListElement_t *element){
	//first check that next->prev and prev->next both point to element
	if (element->next->prev != element || element->prev->next != element) {
		return 1; //corrupted list
	}

	if (opt_yield & DELETE_YIELD) {
		sched_yield();
	}
	

	//remove element from list
	element->next->prev = element->prev;
	element->prev->next = element->next;
	return 0; //element deleted successfully
}

/**
 * SortedList_lookup ... search sorted list for a key
 *
 *	The specified list will be searched for an
 *	element with the specified key.
 *
 * @param SortedList_t *list ... header for the list
 * @param const char * key ... the desired key
 *
 * @return pointer to matching element, or NULL if none is found
 */
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
	if (list == NULL || key == NULL) {
		return NULL;
	}

	SortedListElement_t *curr_elem = list->next;

	while(curr_elem != list) {
		if (strcmp(curr_elem->key, key) == 0) {
			return curr_elem;
		}
		if (opt_yield & LOOKUP_YIELD) {
			sched_yield();
		}
		curr_elem = curr_elem->next;
	}

	return NULL;
}

/**
 * SortedList_length ... count elements in a sorted list
 *	While enumeratign list, it checks all prev/next pointers
 *
 * @param SortedList_t *list ... header for the list
 *
 * @return int number of elements in list (excluding head)
 *	   -1 if the list is corrupted
 */
int SortedList_length(SortedList_t *list) {
	if (list == NULL) {
		return -1;
	}

	SortedListElement_t *curr_elem = list->next;

	int len = 0;
	while (curr_elem != list) { //have not reached end of list
		if (opt_yield & LOOKUP_YIELD) {
			sched_yield();
		}
		len=len + 1;
		curr_elem = curr_elem->next;
	}

	return len;
}

/**/