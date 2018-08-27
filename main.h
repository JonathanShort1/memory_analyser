#ifndef _MAIN_H
#define _MAIN_H

#define TASK_COMM_LEN 16
#define TASK_COMM_ID 0
#define TASK_PID_ID 1
#define TASK_CHILD_ID 2

/**
 * This struct is used to parse the System.map-X file
*/

typedef struct symbol {
    unsigned long long vaddr;
    char symbol[256];
} Map;



/**
 * This struct is for the lime header format
*/

typedef struct lime_header {
	unsigned int magic;
	unsigned int version;
	unsigned long long s_addr;
	unsigned long long e_addr;
	unsigned char reserved[8];
} __attribute__ ((__packed__)) LHdr;


typedef struct lime_header_list {
	LHdr *header;
	struct lime_header_list* next;
} LHdr_list;

LHdr_list* list_add(LHdr_list *list, LHdr *lhdr);


/**
 * This struct is to represent the child doubly linked list 
 * (only parts of it)
 */

struct list_head {
	struct task_struct* next;
	struct task_struct* prev;
} list_head;

/*
 * This struct is for part of the task_struct
*/
struct task_struct {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	struct list_head* children; 
} task_struct;

#endif