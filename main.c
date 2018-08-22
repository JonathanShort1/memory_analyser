#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <linux/sched.h>

#define SYMBOL_SIZE 64
#define SYSTEM_MAP_SIZE 100000

static int debug = 0;

void _debug(const char* msg) {
    if (debug) {
        fprintf(stdout, "DEBUG: %s", msg);
    }
}

const char* sys_filename = "system_map";

typedef struct symbol {
    unsigned long long vaddr;
    char symbol[256];
} Map;

/*
This function opens a file and returns a file pointer
@param filename - char* to name of file
@return a FILE pointer to the file
*/
FILE* open_file(const char* filename) {
    FILE* fp;

    if (!(fp = fopen(filename, "r"))) {
        fprintf(stderr, "Could not open file: %s", filename);
        exit(1);
    }
    
    _debug("Succesful open of System map");

    return fp;
}

/*
This function returns the length of a file using fseek()
@params fp - an open FILE*
@returns n or 0
*/
int file_length(FILE* fp) {
    int fileSize = 0;
    
    fseek(fp, 0L, SEEK_END);
    fileSize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    fileSize == 0 ? _debug("Coultdn't find size of file") : _debug("found size of file");
    
    return fileSize;
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
    unsigned long long paddr;
    //TODO make translation
    return paddr; 
}

/*
This function parses the system map file and returns a pointer 
to an array of symbols
*/
Map** parse_system_map(FILE* fp) {
    int fileSize = file_length(fp);

    char* buff = malloc(sizeof(char) * (fileSize + 2));
    fread(buff, sizeof(char), fileSize,fp);
    buff[fileSize] = '\0';

    Map **map = malloc (SYSTEM_MAP_SIZE * sizeof(Map));

    char *tok_line;
    char *tok_line_end;
    char *tok_space;
    char *tok_space_end;
    char *strtol_ptr;
    tok_line = strtok_r(buff, "\n", &tok_line_end);
    
    int index = 0;
    int i = 0;

    while(tok_line) {
        map[index] = malloc(sizeof(Map));
         
        tok_space = strtok_r(tok_line, " ", &tok_space_end);
        while(tok_space) {
            switch(i) {
                case 0: // Address 
                    map[index]->vaddr = strtol(tok_space, &strtol_ptr, 16);
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

int main(void) {
    if (getuid() != 0) {
        fprintf(stderr, "Run as root!\n");
        exit(1);
    }
    FILE* sysmap_fp = open_file(sys_filename);
    Map** map = parse_system_map(sysmap_fp);
    unsigned long long init_task_addr = get_symbol_vaddr(map, "init_task"); 
    return 0;
}