#ifndef _MAIN_H
#define _MAIN_H

/*
This struct is used to parse the System.map-X file
*/

typedef struct symbol {
    unsigned long long vaddr;
    char symbol[256];
} Map;

typedef struct lime_header {
	unsigned int magic;
	unsigned int version;
	unsigned long long s_addr;
	unsigned long long e_addr;
	unsigned char reserved[8];
} __attribute__ ((__packed__)) LHdr;


#endif