#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
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

off64_t STATIC_SHIFT = 0;
const unsigned long long arrShifts[NUM_Shifts] = {
  0xffff880000000000,
  0xffffffff80000000, 
  0xffffffff80000000 - 0x1000000, 
  0xffffffff7fe00000
};

const off64_t comm_offset = 0x608;
const off64_t pid_offset = 0x450;
const off64_t children_offset = 0x470;
const char* INIT_TASK = "init_task";
// move this to header so can do defines on kerenl version
const char* INIT_PGT = "init_level4_pgt";
const char* INIT_TASK_COMM = "swapper/0";

void _debug(const char *format,...) {
  if (debug) {
    va_list va;
    va_start(va,format);
    vfprintf(stdout,format,va);
    va_end(va);
    printf("\n");
  }
}

void _die(const char *format,...) {
  va_list va;
  va_start(va,format);
  vfprintf(stderr,format,va);
  va_end(va);
  printf("\n");
  exit(1);
}

/* NEEDS TO BE MOVED*/
LHdr_list* list_add(LHdr_list *l, LHdr *h) {
  LHdr_list* new_head = (LHdr_list*) malloc(sizeof(LHdr_list));
  new_head->header = h;
  new_head->next = l;
  l = new_head;
  return l;
}

/**
 * This function allocates memory for a task_struct
 * @params ts - a pointer to a task_struct
*/
void task_struct_init(struct task_struct *ts) {
  ts = malloc(sizeof(struct task_struct));
}

/**
 * This function allocates memory to a list_head
 * @params ts - a pointed to a task_struct
*/
void list_head_init(struct task_struct *ts) {
  ts->children = malloc(sizeof(struct list_head));
}

/**
 * This function opens a file and returns a file descriptor
 * @param filename - char* to name of file
 * @return a FILE descriptor to the file
*/
int open_file(const char* filename) {
  int fd;

  fd = open(filename, O_RDWR); 
  
  if (fd == -1) {
    fprintf(stderr, "Could not open file: %s", filename);
    exit(1);
  }
  
  _debug("DEBUG: Succesful open of file: %s", filename);

  return fd;
}

/**
 * This function returns the length of a file using fseek()
 * @params fd - an open descriptor
 * @returns n or 0
*/
off_t get_file_length(int fd) {
  lseek(fd, 0, SEEK_SET);
  off_t size = lseek(fd, 0, SEEK_END);
  if(size == (off_t) -1) {
    _die("ERROR: Cannot seek to end of file");
  }
  lseek(fd, 0, SEEK_SET);
    
  _debug("DEBUG: found size of file");
  
  return size;
}

/**
 * This function returns the associated virtual address of a symbol 
 * @param map - the map to search 
 * @param symbol - the symbol to search for
 * @returns Either the address if found or -1 if sysmbol isn't present
*/
unsigned long long get_symbol_vaddr(Map** map, const char* symbol) {
  for (int i = 0; i < SYSTEM_MAP_SIZE; i++) {
    if (strcmp(map[i]->symbol, symbol) == 0) {
      return map[i]->vaddr;
    }
  }
  return -1;
}

/**
 * This function fills a linked list of lime headers with data
 * @params fp - file pointer for dump file
 * @return l - the linked list of headers
*/
LHdr_list* get_lime_headers(int fd) {
  LHdr_list *l = calloc(1, sizeof(LHdr_list));
  unsigned long long fileSize = get_file_length(fd);

  if (lseek64(fd, 0, SEEK_SET) != 0) 
    _die("Unable to seek to offset 0 in dump");

  unsigned long long bytes_read = 0;
  int header_count = 0;
  off64_t block_size = 0;
  off64_t seek;
  while (bytes_read < fileSize - 1) {
    LHdr *header = malloc(sizeof(LHdr));
    if (read(fd, header, sizeof(LHdr)) != sizeof(LHdr))
      _die("unable to read in lime header: %d", header_count);
    
    block_size = header->e_addr - header->s_addr + 1;
    seek = lseek64(fd, block_size, SEEK_CUR);
    if (seek == -1) {
      _die("unable to seek to next header"); 
    }

    l = list_add(l, header);
    header_count += 1;
    bytes_read = seek;
  }

  return l;
}

/**
 * This function gets the attr of the task_struct required from the dump
 * (currently assuming seek pointer is a base of task_struct)
 * Assuming task_struct does not go over lime block boundaries
 * @params fd - file descriptor or dump file
 * @params curr - task struct being processed
 * @params offset - offset of attr to read
 * @params length - length of attr to read  
*/
void get_task_attr(int fd, struct task_struct* curr, off64_t offset, int length, int attr) {
  lseek(fd, 0, SEEK_CUR);
  if (lseek64(fd, offset, SEEK_CUR) != -1) {
    int num_read = 0;
    switch(attr) {
      case TASK_COMM_ID:
        num_read = read(fd, curr->comm, TASK_COMM_LEN);
        break;
      case TASK_PID_ID:
        num_read = read(fd, &curr->pid, sizeof(int));
        break;
      case TASK_CHILD_ID:
        num_read = read(fd, curr->children, sizeof(struct list_head));
        break;
      default:
        _die("wrong attr ID provided: %s", attr);
    }
    if (num_read == -1) {
      printf("ERROR: %s, %d\n", strerror(errno), errno);
      _die("Unable to read attr_id: %d", attr);
    }
    lseek64(fd, -(offset + length), SEEK_CUR);
  } else {
    _die("ERROR: Cannot seek to offset: %llx (attr_id: %d)", offset, attr);
  }
}

/**
 * This function finds the correct lime block for the paddr
 * And seeks the dump file pointer to the start of that block
 * (seeks backwards as blocks are linked in reverse order)
 * @params fd - file descriptor of dump
 * @params list - list of lime headers
 * @params paddr - physical address to find in dump blocks
*/
LHdr* get_correct_header_and_seek(int fd, LHdr_list *list, unsigned long long paddr) {
  if (lseek64(fd, 0, SEEK_END) == -1) {
    _die("Unable to seek to end of dump");
  }

  int succFlag = 0;
  int header_length = sizeof(LHdr);
  unsigned long long blockSize = 0;
  LHdr_list *node  = list;
  while (node->next) {
    if (paddr < node->header->e_addr) {
      succFlag = 1;
      break;
    }
    blockSize = node->header->e_addr - node->header->s_addr + 1;
    if (lseek64(fd, -(header_length + blockSize),SEEK_CUR) == -1) {
      _die("unable to seek to next block header");
    }
    node = node->next;
  }
  if (!succFlag) {
    _debug("unable to find correct block in dump for address: %llx", paddr);
    return NULL;
  }

  //seek to header of header
  blockSize = node->header->e_addr - node->header->s_addr + 1;
  if (lseek64(fd, -blockSize, SEEK_CUR) == -1) {
    _die("unable to seek to start of desired block");
  }
  return node->header;
}

/**
 * This function takes the address of init_task and fills a task_struct
 * structure
 * @params fd - file descriptor of dump
 * @params first - first lime header in dump
 * @params second - second line header
 * @params task_struct - the struct to fill
 * @params addr - the address of the struct in the dump
*/
void find_init_task(int fd, LHdr_list *list, struct task_struct *ts, unsigned long long vaddr) {
  int successShiftFlag = 0;
  long long header_length = sizeof(LHdr);

  //find the correct shift
  unsigned long long paddr = 0;
  for (int i = 0; i < NUM_Shifts; i++) {
    if (vaddr >= arrShifts[i]) {
      paddr = vaddr - arrShifts[i];

      LHdr* l = get_correct_header_and_seek(fd, list, paddr);
     
      if (l && paddr <= l->e_addr) {
        if (lseek64(fd, paddr - l->s_addr, SEEK_CUR) != -1) {
          if (lseek64(fd, comm_offset, SEEK_CUR) != -1) {
            if (read(fd, ts->comm, TASK_COMM_LEN -1) != -1) {
              if (strcmp(ts->comm, INIT_TASK_COMM) == 0) {
                _debug("SUCESS: found a viable static shift");
                successShiftFlag = 1;
                STATIC_SHIFT = arrShifts[i];
                break; // found correct shift
              }
            } else {
              _debug("Unable to read in the comm of task");
            }
          } else {
            _debug("Unable to seek to comm_offset");
          }
        } else {
          _debug("Unable to seek to init_task with shift chosen");
        }
      } else {
        _debug("address to big for block chosen");
      }
    }
  }
  // fill rest of task if found 
  if (successShiftFlag) {
      if (lseek64(fd, -(comm_offset + TASK_COMM_LEN), SEEK_CUR) != -1) {
        // find and read pid
        get_task_attr(fd, ts, pid_offset, sizeof(int), TASK_PID_ID);

        //find and read children list_head
        list_head_init(ts);
        get_task_attr(fd, ts, children_offset, sizeof(list_head), TASK_CHILD_ID);
      } else {
        _die("could not seek back to statr of task struct");
      }
    } else {
      _die("Could not find a successful shift!");
    }
}

/**
 * This function translates a virtual address to a physical address
 * (takes longer if static offset is not set)
 * (only works for nokaslr sofar)
 * @params vaddr - the virtual address to be translated
*/
off64_t paddr_translation(off64_t vaddr) {
  if (STATIC_SHIFT) {
    if(vaddr >= STATIC_SHIFT) {
      return vaddr - STATIC_SHIFT;
    }
    _die("Translation of addr: %llx with shift: %lxx could cause overflow", vaddr, STATIC_SHIFT);
  } else {
    //TODO change this to incorp some of find_task
    _die("STATIC offset not set");
  }
  return -1;
}

/**
 * This function seeks to the base of the given task
 * @params fd - file descriptor of dump file
 * @params vaddr - virtual address of base 
*/
void seek_to_base_of_task(int fd, off64_t vaddr) {
  off64_t paddr = paddr_translation(vaddr);

}

/**
 * This function prints out all the processes in the task_struct list starting at
 * the task_struct passed
 * @params ts - the task_struct to be pased first
*/
void print_process_list(int fd, struct task_struct *init) {
    printf("comm: %s - pid: %d - child: %p\n", init->comm, init->pid, init->children);
    struct task_struct* curr = init;

    seek_to_base_of_task(fd, curr->children->next);
}

/**
 * This function parses the system map file and returns a pointer 
 * to an array of symbols
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

/**
 * This is the "main" processing function to process the dump
 * @params sys_filename - the filename of the System.map-$(uname -r)
 * @params dump_filename - the name of the memory dump
*/
void process_dump(const char*sys_filename, const char* dump_filename) {
  int sysmap_fd = open_file(sys_filename);
  Map** map = parse_system_map(sysmap_fd);
  unsigned long long init_task_vaddr = (get_symbol_vaddr(map, INIT_TASK));
  // unsigned long long pgt_vaddr = get_symbol_vaddr(map, INIT_PGT);
  
  int dump_fd = open_file(dump_filename);
  //TODO MAKE AN ARRAY OF HEADERS OR VARIABLE NUM OF HEADERS
  LHdr_list *headerList = get_lime_headers(dump_fd);
  
  struct task_struct init_task;
  task_struct_init(&init_task);
  find_init_task(dump_fd, headerList, &init_task, init_task_vaddr);
  //print_process_list(dump_fd, &init_task);
}

/**
 * This functions handles command line arguments
 * 
 * usage: 
 *   sudo ./main -s /PathTo/System.map-$(uname -r) -d /PathTo/memoryDump
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