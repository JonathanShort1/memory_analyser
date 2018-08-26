#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/sched.h>

#include "main.h"

#define SYMBOL_SIZE 64
#define SYSTEM_MAP_SIZE 100000
#define STATIC_OFFSET 0xffff880000000000

static int debug = 0;

const char* sys_filename = "system_map";
const char* dump_filename = "out.bin";

void _debug(const char* msg) {
    if (debug) {
        fprintf(stdout, "DEBUG: %s\n", msg);
    }
}

void _die(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    exit(1);
}


/*
This function opens a file and returns a file descriptor
@param filename - char* to name of file
@return a FILE descriptor to the file
*/
int open_file(const char* filename) {
    int fd;

    fd = open(filename, O_RDWR); 
    
    if (fd == -1) {
        fprintf(stderr, "Could not open file: %s", filename);
        exit(1);
    }
    
    _debug("Succesful open of file");

    return fd;
}

/*
This function returns the length of a file using fseek()
@params fd - an open descriptor
@returns n or 0
*/
off_t get_file_length(int fd) {
    lseek(fd, 0, SEEK_SET);
    off_t size = lseek(fd, 0, SEEK_END);
    if(size == (off_t) -1) {
        _die("Cannot seek");
    }
    lseek(fd, 0, SEEK_SET);
     
    _debug("found size of file");
    
    return size;
}

/* 
This function returns the associated virtual address of a symbol 
@param map - the map to search 
@param symbol - the symbol to search for
@returns Either the address if found or -1 if sysmbol isn't present
*/
unsigned long long get_symbol_vaddr(Map** map, const char* symbol) {
    for (int i = 0; i < SYSTEM_MAP_SIZE; i++) {
        if (strcmp(map[i]->symbol, symbol) == 0) {
            return map[i]->vaddr;
        }
    }
    return -1;
}

/*
This function returns the physical address from the given virtual address
@params vaddr - the virtual address of the symbol
@params paddr - the physical address of the symbol or -1
*/
unsigned long long get_symbol_paddr(const unsigned long long vaddr) {
    //checking for potential overflow
    /*if (vaddr > STATIC_OFFSET) {
        return vaddr - STATIC_OFFSET;
    }*/
    if (vaddr > 0xffffffff7fe00000) {
        return vaddr - 0xffffffff7fe00000;
    }
    return -1; 
}

/*
This function fills two struct is data for Lime headers
@params fp - file pointer for dump file
@params first_LHdr - first lime header in dump
@params second_LHdr - second lime header in dump
@returns fills the above structs or they return null on failure
*/
void get_lime_headers(int fd, LHdr *first_lhdr, LHdr *second_lhdr) {
    if (lseek64(fd, 0, SEEK_SET) != 0) 
        _die("Unable to seek to offset 0 in dump");

    if (read(fd, first_lhdr, sizeof(LHdr)) != sizeof(LHdr))
        _die("unable to read in first lime header");
    
    off64_t block_size = first_lhdr->e_addr - first_lhdr->s_addr + 1;

    off64_t seek = lseek64(fd, block_size, SEEK_CUR); 
    long header_length = sizeof(LHdr);
    if (seek != block_size + header_length) {
        _die("unable to seek to the second header"); 
    }

    if (read(fd, second_lhdr, sizeof(LHdr)) != sizeof(LHdr))
        _die("unable to read in second lime header");

}

/*
This function takes the address of init_task and fills a task_struct
structure
@params fd - file descriptor of dump
@params first - first lime header in dump
@params second - second line header
@params task_struct - the struct to fill
@params addr - the address of the struct in the dump
*/
void find_init_task(int fd, LHdr *first, LHdr *second, struct task_struct *ts, unsigned long long addr) {
    // find which header to use
    LHdr* l = first;
    if(addr > first->e_addr) {
        l = second;
    }

    off64_t block_size = first->e_addr - first->s_addr + 1;
    long long header_length = sizeof(LHdr);
    if (lseek64(fd, block_size + (2 * header_length), SEEK_SET) != (block_size + (2 * header_length))) {
        _die("unable to seek to correct block");
    }

    if (addr > l->e_addr) {
        _die("Address greater than block size");
    }

    off64_t seek = lseek64(fd, addr - l->s_addr, SEEK_CUR);
    if (seek == -1) {
        _die("unable to seek to init_task");
    }

    // read(fd, ts, sizeof());


    //lseek to correct location
    //fill task
}

/*
This function parses the system map file and returns a pointer 
to an array of symbols
*/
Map** parse_system_map(int fd) {
    off_t fileSize = get_file_length(fd);
    char* buff = malloc(sizeof(char) * (fileSize + 2));
    if ((read(fd, buff,fileSize) == -1)) {
        fprintf(stderr, "Unable to system map\n");
    }
    buff[fileSize] = '\0';

    Map **map = malloc (SYSTEM_MAP_SIZE * sizeof(Map));

    char *tok_line;
    char *tok_line_end;
    char *tok_space;
    char *tok_space_end;
    char *strtol_ptr;
    tok_line = strtok_r(buff, "\n", &tok_line_end);
    
    int index = 0; //line being parsed
    int i = 0; // position in line 0, 1, 2 -> addr, type, sym

    while(tok_line) {
        map[index] = malloc(sizeof(Map));
        tok_space = strtok_r(tok_line, " ", &tok_space_end);
        while(tok_space) {
            switch(i) {
                case 0: // Address 
                    map[index]->vaddr = strtoull(tok_space, &strtol_ptr, 16);
                    break;
                case 1:
                    // not used
                    break;
                case 2: // Symbol
                    strncpy(map[index]->symbol, tok_space, SYMBOL_SIZE);
                    break;
                default:
                    fprintf(stderr, "Error parsing a symbol struct\n");
            }
            i += 1;
            tok_space = strtok_r(NULL, " ", &tok_space_end);
        }
        i = 0;
        index += 1;
        tok_line = strtok_r(NULL, "\n", &tok_line_end);
    }

    return map;
}

void print_process_list(struct task_struct *ts) {
    
}

int main(void) {
    if (getuid() != 0) {
        fprintf(stderr, "Run as root!\n");
        exit(1);
    }
    int sysmap_fd = open_file(sys_filename);
    Map** map = parse_system_map(sysmap_fd);
    unsigned long long init_task_addr = get_symbol_paddr(get_symbol_vaddr(map, "init_task"));
    printf("address of init task: %llx\n", init_task_addr);
    printf("vaddr of init: %llx\n", get_symbol_vaddr(map, "init_task"));
    
    int dump_fd = open_file(dump_filename);
    //MAKE AN ARRAY OF HEADERS OR VARIABLE NUM OF HEADERS
    LHdr first_lhdr;
    LHdr second_lhdr;
    get_lime_headers(dump_fd, &first_lhdr, &second_lhdr);
    
    struct task_struct *init_task = NULL;
    find_init_task(dump_fd, &first_lhdr, &second_lhdr, init_task, init_task_addr);
    print_process_list(init_task);
    return 0;
}