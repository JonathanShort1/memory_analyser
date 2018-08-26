#ifndef _MAIN_H
#define _MAIN_H

#define TASK_COMM_LEN 16

/*
This struct is used to parse the System.map-X file
*/

typedef struct symbol {
    unsigned long long vaddr;
    char symbol[256];
} Map;

/*
This struct is for the lime header format
*/

typedef struct lime_header {
	unsigned int magic;
	unsigned int version;
	unsigned long long s_addr;
	unsigned long long e_addr;
	unsigned char reserved[8];
} __attribute__ ((__packed__)) LHdr;

/*
This struct is for part of the task_struct
*/
struct task_struct {
	pid_t pid;
	char comm[TASK_COMM_LEN];
} task_struct;

#endif