#ifndef _MAIN_H
#define _MAIN_H

#define TASK_COMM_LEN 16
#define TASK_PID_LEN sizeof(int)
#define TASK_TASKS_LEN sizeof(struct list_head)
#define TASK_PARENT_PTR_LEN sizeof(struct task_struct *)

#define TASK_COMM_ID 0
#define TASK_PID_ID 1
#define TASK_TASKS_ID 2
#define TASK_PARENT_PTR_ID 3
#define TASK_PPID_ID 4


/**
 * This struct is used to parse the System.map-X file
*/

typedef struct symbol {
	  char symbol[256];
    unsigned long long vaddr;
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
	unsigned long long block_s_offset;
	unsigned long long block_e_offset;
	struct lime_header_list* next;
} LHdr_list;

LHdr_list* header_list_add(LHdr_list *list, LHdr *lhdr);

/*
 * This struct is for part of the task_struct
*/

struct list_head {
	struct list_head *next, *prev;
}; 

struct task_struct {
	pid_t pid;
	pid_t ppid;
	char comm[TASK_COMM_LEN];
	struct list_head tasks;
	struct task_struct* parent_ptr;
} task_struct;

#endif