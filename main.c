#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/sched.h>

#include "main.h"

#define SYMBOL_SIZE 64
#define SYSTEM_MAP_SIZE 100000
#define NUM_Shifts 4

/* DEBUG FLAG */
static int debug = 1;

unsigned long long STATIC_SHIFT = 0;
const unsigned long long arrShifts[NUM_Shifts] = {
  0xffff880000000000,
  0xffffffff80000000, 
  0xffffffff80000000 - 0x1000000, 
  0xffffffff7fe00000
};

unsigned long long comm_offset = 0x608;
unsigned long long pid_offset = 0x450;
const char* INIT_TASK = "init_task";
// move this to header so can do defines on kerenl version
const char* INIT_PGT = "init_level4_pgt";
const char* INIT_TASK_COMM = "swapper/0";



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
void find_init_task(int fd, LHdr *first, LHdr *second, struct task_struct *ts, unsigned long long vaddr) {
  int successShiftFlag = 0;
  long long header_length = sizeof(LHdr);

  //find the correct shift
  unsigned long long paddr = 0;
  for (int i = 0; i < NUM_Shifts; i++) {
    if (vaddr >= arrShifts[i]) {
      paddr = vaddr - arrShifts[i];

      LHdr* l = first;
      if(paddr < first->e_addr) {
        if (lseek64(fd, header_length, SEEK_SET) != header_length) {
          _die("Unable to seek to correct block");
        }
      } else {
        l = second;
        
        off64_t block_size = first->e_addr - first->s_addr + 1;
        if (lseek64(fd, block_size + (2 * header_length), SEEK_SET) != (block_size + (2 * header_length))) {
          _die("unable to seek to correct block");
        }
      }

      if (paddr > l->e_addr) {
        _debug("Address greater than block size");
      } else {
        printf("shift: %llx\n", arrShifts[i]);
        if (lseek64(fd, paddr - l->s_addr, SEEK_CUR) != -1) {
          if (lseek64(fd, comm_offset, SEEK_CUR) != -1) {
            if (read(fd, ts->comm, TASK_COMM_LEN -1) != -1) {
              if (strcmp(ts->comm, INIT_TASK_COMM) == 0) {
                successShiftFlag = 1;
                STATIC_SHIFT = arrShifts[i];
                break; // found correct shift
              }
            } else {
              printf("Error: %s, erron: %d\n", strerror(errno), errno);
              _debug("Unable to read in the comm of task");
            }
          } else {
            _debug("Unable to seek to comm_offset");
          }
        } else {
          _debug("Unable to seek to init_task with shift chosen");
        }
      }
    }
  }
  // fill rest of task if found 
  if (successShiftFlag) {
      if (lseek64(fd, pid_offset -(comm_offset + TASK_COMM_LEN), SEEK_CUR) != -1) {
          if(read(fd, &ts->pid, sizeof(int))!= -1) {
            printf("pid: %d\n", ts->pid);
          } else {
          _die("Could not read init_task pid");
          }
      } else {
        _die("could not seek back to init_task pid offset");
      }
    } else {
      _die("Could not find a successful shift!");
    }

  // read in the pid and comm
  // test for correct values 
  //print the values
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

/*
This function prints out all the processes in the task_struct list starting at
the task_struct passed
@params ts - the task_struct to be pased first
*/
void print_process_list(struct task_struct *ts) {
    
}
/*
This is the "main" processing function to process the dump
@params sys_filename - the filename of the System.map-$(uname -r)
@params dump_filename - the name of the memory dump
*/
void process_dump(const char*sys_filename, const char* dump_filename) {
  int sysmap_fd = open_file(sys_filename);
  Map** map = parse_system_map(sysmap_fd);
  unsigned long long init_task_vaddr = (get_symbol_vaddr(map, INIT_TASK));
  // unsigned long long pgt_vaddr = get_symbol_vaddr(map, INIT_PGT);
  
  int dump_fd = open_file(dump_filename);
  //TODO MAKE AN ARRAY OF HEADERS OR VARIABLE NUM OF HEADERS
  LHdr first_lhdr;
  LHdr second_lhdr;
  get_lime_headers(dump_fd, &first_lhdr, &second_lhdr);
  
  struct task_struct *init_task = malloc(sizeof(struct task_struct));
  find_init_task(dump_fd, &first_lhdr, &second_lhdr, init_task, init_task_vaddr);
  // print_process_list(init_task);
}

/*
This functions handles command line arguments 

usage: 
  sudo ./main -s /PathTo/System.map-$(uname -r) -d /PathTo/memoryDump
*/
int main(int argc, char** argv) {
  if (getuid() != 0) {
    fprintf(stderr, "Run as root!\n");
    exit(1);
  }

  char* sys_filename = NULL;
  char* dump_filename = NULL;
  
  int sflag = 0;
  int dflag = 0;
  int opt = 0;

  while((opt = getopt (argc, argv, "s:d:"))!= -1) {
    switch(opt) {
      case 's':
        sflag = 1;
        sys_filename = optarg;
        break;
      case 'd':
        dflag = 1;
        dump_filename = optarg;
        break;
      case ':': /* Fall through is intentional */
      case '?': /* Fall through is intentional */
      default:
        printf("Invalid options or missing argument: '-%c'.\n",
            opt);
        break;
    }
  }

  if (!sflag || !dflag) {
    _die("did not pass system file name or dump filename");
  }

  process_dump(sys_filename, dump_filename);
  
  return 0;
}