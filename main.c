#define _LARGEFILE64_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/sched.h>

#include "main.h"

#define SYMBOL_SIZE 64
#define SYSTEM_MAP_SIZE 100000
#define NUM_Shifts 4

#define PAGE_MAP_MASK 0x0000FF8000000000
#define PDPT_MASK     0x0000007FC0000000
#define PDE_MASK      0x000000003FE00000
#define PTE_MASK      0x00000000001FF000
#define PAGE_OFF_MASK 0x0000000000000FFF


// move this to header so can do defines on kerenl version
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 13, 0)
const char* INIT_PGT = "init_level4_pgt";
#else
const char* INIT_PGT = "init_pgt";
#endif

#define INIT_TASK "init_task"
#define INIT_TASK_COMM "swapper/0"

/* DEBUG FLAG */
static int debug = 0;

unsigned long long KERNEL_MAP_SHIFT = 0;
unsigned long long STATIC_SHIFT = 0xffff880000000000;
unsigned long long PGT_PADDR = 0;
const unsigned long long arrShifts[NUM_Shifts] = {
  0xffff880000000000,
  0xffffffff80000000, 
  0xffffffff80000000 - 0x1000000, 
  0xffffffff7fe00000
};

const unsigned long long comm_offset = 0x608;
const unsigned long long pid_offset = 0x450;
const unsigned long long tasks_offset = 0x358 + 1;
const unsigned long long parent_offset = 0x468 + 1;

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
/**
 * This function adds to the head of a linkedlist of lime headers
 * These headers are used to seek to the correct block (and address) in the dump file
 * @params l - the head of the list
 * @params h - the lime header to be added to the list
 * @returns l - the updated head of the list
*/
LHdr_list* header_list_add(LHdr_list *l, LHdr *h) {
  LHdr_list* new_head = (LHdr_list*) malloc(sizeof(LHdr_list));
  new_head->header = h;
  new_head->next = l;
  l = new_head;
  return l;
}

/**
 * This function allocates memory for a task_struct
 * @params ts - a pointer to a task_struct
 * @returns task_struct * - an allocated pointer to a task struct
*/
struct task_struct* task_struct_init() {
  return malloc(sizeof(struct task_struct));
}

/**
 * ****************************************************
 * FILE PROCESSING
 * ****************************************************
*/

/**
 * This function opens a file and returns a file descriptor
 * @param filename - char* to name of file
 * @return a FILE descriptor to the file
*/
int open_file(const char* filename) {
  int fd;

  fd = open(filename, O_RDWR); 
  if (fd == -1) {
   _die("Could not open file: %s", filename);
  }
  _debug("DEBUG: Succesful open of file: %s", filename);
  return fd;
}

/**
 * This function returns the length of a file using fseek()
 * @params fd - an open descriptor
 * @returns n or 0
*/
unsigned long long get_file_length(int fd) {
  lseek(fd, 0, SEEK_SET);
  unsigned long long size = lseek(fd, 0, SEEK_END);
  if(size == (unsigned long long) -1) {
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
 * It seeks to the end of the dump file without resetting it 
 * 
 * It also sets PA_MAX to the e_addr of the last header
 * 
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
  unsigned long long block_size = 0;
  off64_t seek;
  while (bytes_read < fileSize - 1) {
    LHdr *header = malloc(sizeof(LHdr));
    if (read(fd, header, sizeof(LHdr)) != sizeof(LHdr)) {
      _die("Unable to read in lime header: %d", header_count);
    }
    
    l = header_list_add(l, header);
    l->block_s_offset = lseek64(fd, 0, SEEK_CUR);
    
    block_size = header->e_addr - header->s_addr + 1;
    seek = lseek64(fd, block_size, SEEK_CUR);
    if (seek == -1) {
      _die("Unable to seek to next header"); 
    }
    l->block_e_offset = lseek64(fd, 0, SEEK_CUR);

    header_count += 1;
    bytes_read = seek;
  }

  if (debug) {
    LHdr_list *curr = l;
    while (curr->next) {
      printf("start: %llx, end: %llx\n", curr->header->s_addr, curr->header->e_addr);
      curr = curr->next;
    }
  }
  
  return l;
}

/**
 * This function gets the attr of the task_struct required from the dump
 * (currently assuming seek pointer is a base of task_struct)
 * Assuming task_struct does not go over lime block boundaries
 * Restores seek offset
 * @params fd - file descriptor or dump file
 * @params curr - task struct being processed
 * @params offset - offset of attr to read
 * @params length - length of attr to read  
*/
void get_task_attr(int fd, struct task_struct* curr, unsigned long long  offset, int length, int attr) {
  off64_t seek_pre_process = lseek64(fd, 0, SEEK_CUR);
  if (lseek64(fd, offset, SEEK_CUR) != -1) {
    int num_read = 0;
    switch(attr) {
      case TASK_COMM_ID:
        num_read = read(fd, curr->comm, length);
        break;
      case TASK_PID_ID:
        num_read = read(fd, &curr->pid, length);
        break;
      case TASK_PARENT_PTR_ID:
        num_read = read(fd, &curr->parent_ptr, length);
        break;
      case TASK_TASKS_ID:
        num_read = read(fd, &curr->tasks, length);
        break;
      case TASK_PPID_ID:
        num_read = read(fd, &curr->ppid, length);
        break;
      default:
        _die("get_task_attr - Wrong attr ID provided: %d", attr);
    }
    if (num_read == -1) {
      _die("get_task_attr - Unable to read attr_id: %d", attr);
    }
    if (lseek64(fd, -(offset + length), SEEK_CUR) != seek_pre_process) {
      _die("get_task_attr - Unable to restore file seek");
    }
  } else {
    _die("get_task_attr - Cannot seek to offset: %llx (attr_id: %d)", offset, attr);
  }
}



/**
 * This function finds the correct lime block for the paddr and seeks to that paddr
 * (seeks backwards as blocks are linked in reverse order)
 * Does not restore seek offset
 * @params fd - file descriptor of dump
 * @params list - list of lime headers
 * @params paddr - physical address to find in dump blocks
 * @returns -1 on failure or 0 for success
*/
int paddr_to_offset(int fd, LHdr_list *list, unsigned long long paddr) {
  if (lseek64(fd, 0, SEEK_END) == -1) {
    _die("Unable to seek to end of dump");
  }

  int succFlag = 0;
  int header_length = sizeof(LHdr);
  unsigned long long blockSize = 0;
  LHdr_list *node  = list;
  while (node->next) {
    if (node->header->s_addr <= paddr && paddr < node->header->e_addr) {
      succFlag = 1;
      break;
    }
    blockSize = node->header->e_addr - node->header->s_addr + 1;
    if (lseek64(fd, -(header_length + blockSize),SEEK_CUR) == -1) {
      _die("get_correct_header_and_seek - Unable to seek to next block header");
    }
    node = node->next;
  }
  if (!succFlag) {
    _debug("DEBUG: unable to find correct block in dump for address: %llx", paddr);
    return -1;
  }

  //seek to paddr in block
  blockSize = node->header->e_addr - node->header->s_addr + 1;
  if (lseek64(fd, -blockSize + (paddr - node->header->s_addr), SEEK_CUR) == -1) {
    _die("get_correct_header_and_seek - Unable to seek to start of desired block");
  }
  return lseek64(fd, 0, SEEK_CUR);
}

/**
 * This function takes an offset from the dump file
 * and converts it to the physical address 
 * Restores seek 
 * @params fd - file descriptor of dump
 * @params list - list o lime headers
 * @params seek - position of seek offset in dump file
 * @returns paddr - the physical address
*/
unsigned long long offset_to_paddr(LHdr_list *list, unsigned long long seek) {
  int foundBlockFlag = 0;
  LHdr_list *node = list;
  while (node->next) {
    if (node->block_s_offset <= seek && seek < node->block_e_offset) {
        foundBlockFlag = 1;
        break;
    } 
    node = node->next;
  }
  if (!foundBlockFlag) {
    _die("Unable to find block that contains offset: %llx", seek);
  }

  return = seek + node->header->e_addr + 1 - node->block_e_offset;
}

/**
 * This function returns the pid of a process' parent
 * @params fd - file descriptor of dump
 * @params list - list of lime headers in dump file
 * @parms curr - the current task to find the parent pid of 
*/ 
void get_parent_pid(int fd, LHdr_list *list, struct task_struct* curr) {
  unsigned long long parent_paddr;
  unsigned long long parent_vaddr = (unsigned long long) curr->parent_ptr;
  if (parent_vaddr > KERNEL_MAP_SHIFT) {
    parent_paddr = parent_vaddr - KERNEL_MAP_SHIFT;
  } else {
    parent_paddr = parent_vaddr - STATIC_SHIFT;
  }
  paddr_to_offset(fd, list, parent_paddr);

  get_task_attr(fd, curr, pid_offset, TASK_PID_LEN, TASK_PPID_ID);
}

/**
 * This function takes the address of init_task and fills a task_struct
 * structure
 * Does not restore seek offset
 * @params fd - file descriptor of dump
 * @params first - first lime header in dump
 * @params second - second line header
 * @params task_struct - the struct to fill
 * @params addr - the address of the struct in the dump
*/
void find_init_task(int fd, LHdr_list *list, struct task_struct *ts, unsigned long long vaddr) {
  int successShiftFlag = 0;

  //find the correct shift
  unsigned long long paddr = 0;
  for (int i = 0; i < NUM_Shifts; i++) {
    if (vaddr >= arrShifts[i]) {
      paddr = vaddr - arrShifts[i];

      int seek = paddr_to_offset(fd, list, paddr);
      if (seek != -1) {
        if (lseek64(fd, comm_offset, SEEK_CUR) != -1) {
          if (read(fd, ts->comm, TASK_COMM_LEN -1) != -1) {
            if (strcmp(ts->comm, INIT_TASK_COMM) == 0) {
              _debug("SUCCESS: found a viable static shift: %llx", arrShifts[i]);
              successShiftFlag = 1;
              KERNEL_MAP_SHIFT = arrShifts[i];
              break; // found correct shift
            }
          } else {
            _debug("DEBUG: Unable to read in the comm of task");
          }
        } else {
          _debug("DEBUG: Unable to seek to comm_offset");
        }
      } else {
        _debug("DEBUG: address not in dump");
      }
    }
  }

  // fill rest of task if found 
  if (successShiftFlag) {
      if (lseek64(fd, -(comm_offset + TASK_COMM_LEN), SEEK_CUR) != -1) { // seek back to base of task_struct
        get_task_attr(fd, ts, pid_offset, TASK_PID_LEN, TASK_PID_ID); // find and read pid
        get_task_attr(fd, ts, tasks_offset, TASK_TASKS_LEN, TASK_TASKS_ID);  // find and read tasks list_head
        get_task_attr(fd, ts, parent_offset, TASK_PARENT_PTR_LEN, TASK_PARENT_PTR_ID);
        get_parent_pid(fd, list, ts);
      } else {
        _die("Could not seek back to start of task struct");
      }
    } else {
      _die("Could not find a successful shift!");
    }
}

/**
 * ****************************************************
 * PRINTING PROCESSES
 * ****************************************************  
*/

/**
 * This function converts a ULL to little endian (and vice versa)
 * Then trims any trailing zeros e.g. 0x687000 -> 0x687
 * @params x - the ull to change
 * @returns the new representation of the ULL 
 */ 
unsigned long long to_little_endian(unsigned long long x) {
  unsigned long long y = (
    ((x >> 56) & 0x00000000000000ff) |
    ((x >> 40) & 0x000000000000ff00) |
    ((x >> 24) & 0x0000000000ff0000) |
    ((x >> 8) &  0x00000000ff000000) | 
    ((x << 8) &  0x000000ff00000000) | 
    ((x << 24) & 0x0000ff0000000000) |
    ((x << 40) & 0x00ff000000000000) |
    ((x << 56) & 0xff00000000000000)
    );

  int byte;
  while (y != 0 && (byte = (y & 0xFF)) == 0) {
    y = y >> 8;
  }
  return y;
}

/**
 * This function translates a virtual address to a physical address
 * (Cannot be used if static offset - use get_lime_headers)
 * (only works for nokaslr so far)
 * SAFE TO SEEK
 * @params fd - file descriptor
 * @params list - list of lime headers
 * @params vaddr - the virtual address to be translated
 * @returns paddr - the physical address or -1 on failure 
*/
unsigned long long paddr_translation(int fd, LHdr_list *list, unsigned long long vaddr) {
  if (!KERNEL_MAP_SHIFT) {
    _die("STATIC SHIFT not set");
  }
  
  if (vaddr > KERNEL_MAP_SHIFT) {
    return vaddr - KERNEL_MAP_SHIFT;
  }

  //return vaddr - 0xffff880000000000;

  unsigned long long pa_pdpte = 0;
  unsigned long long pa_pde = 0;
  unsigned long long pa_pte = 0;
  unsigned long long pa_page = 0;

  unsigned int pgt_offset = (vaddr & PAGE_MAP_MASK) >> 39;
  unsigned int pdpt_offset = (vaddr & PDPT_MASK) >> 30;
  unsigned int pde_offset = (vaddr & PDE_MASK) >> 21;
  unsigned int pte_offset = (vaddr & PTE_MASK) >> 12;
  unsigned int page_offset = vaddr & PAGE_OFF_MASK;

  /* read address of page directory pointer table */
  paddr_to_offset(fd, list, PGT_PADDR + (8 * pgt_offset));
  printf("\n\npme: %lx\n", lseek64(fd, 0, SEEK_CUR));
  if (read(fd, &pa_pdpte, sizeof(unsigned long long)) == -1) {
    _die("Failure to read in pa_pdpt: %llx", PGT_PADDR + (8 * pgt_offset));
  }
  printf("pa_pdpt: %llx, %llx\n", pa_pdpte, to_little_endian(pa_pdpte));

  /* read address of page directory */
  paddr_to_offset(fd, list, pa_pdpte + (8 * pdpt_offset));
  printf("\n\npa_pdee: %lx\n", lseek64(fd, 0, SEEK_CUR));
  if (read(fd, &pa_pde, sizeof(unsigned long long)) == -1) {
    _die("Failure to read in pa_pdpt: %llx", pa_pdpte + (8 * pdpt_offset));
  }
  printf("pa_pde: %llx, %llx\n",pa_pde, to_little_endian(pa_pde));

  /* read address of page tabe */
  paddr_to_offset(fd, list, to_little_endian(pa_pde) + (8 * pde_offset));
  if (read(fd, &pa_pte, sizeof(unsigned long long)) == -1) {
    _die("Failure to read in pa_pdpt: %llx", pa_pde + (8 * pde_offset));
  }
  printf("pa_pte: %llx, %llx\n", pa_pte, to_little_endian(pa_pte));

  /* read address of page */
  paddr_to_offset(fd, list, to_little_endian(pa_pte) + (8 * pte_offset));
  if (read(fd, &pa_page, sizeof(unsigned long long)) == -1) {
    _die("Failure to read in pa_pdpt: %llx", pa_pte + (8 * pte_offset));
  }
  printf("pa_page: %llx, %llx\n",pa_page, to_little_endian(pa_page));

  return to_little_endian(pa_page) + (8 * page_offset);
}



/**
 * This function prints out all the processes in the task_struct list starting at
 * the task_struct passed
 * @params ts - the task_struct to be pased first
*/
void print_process_list(int fd, LHdr_list *list, struct task_struct *init_task) {
  int isSwapperflag = 1;
  struct task_struct curr;
  task_struct_init(&curr);

  curr.pid = init_task->pid;
  curr.ppid = init_task->ppid;
  strncpy(curr.comm, init_task->comm, TASK_COMM_LEN - 1);
  memcpy(&curr.tasks, &init_task->tasks, sizeof(struct list_head));
  memcpy(&curr.parent_ptr, &init_task->parent_ptr, sizeof(struct task_struct *));

  printf(" Name%*sPID%*sPPID%*sNext Task Addr%*sParent Task Addr\n", 
    15, " ", 4, " ", 4, " ", 8, " ");
  printf("==============================================================================\n");

  unsigned long long next_addr;
  while(curr.pid != 0 || isSwapperflag) {
    printf("%-20s %-6d %-6d %p %p\n", 
      curr.comm, curr.pid, curr.ppid, curr.tasks.next, curr.parent_ptr);
    
    next_addr = (unsigned long long) curr.tasks.next - STATIC_SHIFT;
    if (paddr_to_offset(fd, list, next_addr - tasks_offset + 1) == -1) {
      break; // reached swapper/0
    }

    get_task_attr(fd, &curr, comm_offset, TASK_COMM_LEN, TASK_COMM_ID);
    get_task_attr(fd, &curr, pid_offset, TASK_PID_LEN, TASK_PID_ID);
    get_task_attr(fd, &curr, tasks_offset - 1, TASK_TASKS_LEN, TASK_TASKS_ID);
    get_task_attr(fd, &curr, parent_offset - 1, TASK_PARENT_PTR_LEN, TASK_PARENT_PTR_ID);
    get_parent_pid(fd, list, &curr);
        
    isSwapperflag = 0;
  }
}

/**
 * ****************************************************
 * FILE PARSING
 * ****************************************************
*/

/**
 * This function parses the system map file and returns a pointer 
 * to an array of symbols
 * Closes the file descriptor on exit!
 * @params fd - file descriptor of system map
 * @return an array of symbol structs - struct symbol { char* : symbol, ull : vaddr}
*/
Map** parse_system_map(int fd) {
  unsigned long long fileSize = get_file_length(fd);
  char* buff = malloc(sizeof(char) * (fileSize + 2));
  if ((read(fd, buff,fileSize) == -1)) {
    _die("parse_system_map - Unable to read in system map");
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
          _debug("DEBUG: Error parsing a symbol struct\n");
      }
      i += 1;
      tok_space = strtok_r(NULL, " ", &tok_space_end);
    }
    i = 0;
    index += 1;
    tok_line = strtok_r(NULL, "\n", &tok_line_end);
  }

  close(fd);
  return map;
}

/**
 * This is the "main" processing function to process the dump
 * @params sys_filename - the filename of the System.map-$(uname -r)
 * @params dump_filename - the name of the memory dump
*/
void process_dump(const char* sys_filename, const char* dump_filename) {
  /* open map file and load into array */
  int sysmap_fd = open_file(sys_filename);
  Map** map = parse_system_map(sysmap_fd);
  
  /* open dump file and create linked list of lime headers*/
  int dump_fd = open_file(dump_filename);
  LHdr_list *headerList = get_lime_headers(dump_fd);
  
  /* find and fill the init_task task_struct */
  struct task_struct init_task;
  task_struct_init(&init_task);
  unsigned long long init_task_vaddr = get_symbol_vaddr(map, INIT_TASK);
  find_init_task(dump_fd, headerList, &init_task, init_task_vaddr);
  
  /* set the physical address of the page tables */
  unsigned long long pgt_vaddr = get_symbol_vaddr(map, INIT_PGT);
  PGT_PADDR = pgt_vaddr - KERNEL_MAP_SHIFT;

  /* printf the process list */
  print_process_list(dump_fd, headerList, &init_task);
}

/**
 * This functions handles command line arguments
 * 
 * usage: 
 *   sudo ./main -s /PathTo/System.map-$(uname -r) -d /PathTo/memoryDump
*/
int main(int argc, char** argv) {
  // if (getuid() != 0) {
  //   _die("Run as root!\n");
  // }

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

  char* usage = "Usage: sudo ./main -s /path/to/System.map -d /path/to/dump\n\n";
  if (!sflag || !dflag) {
    _die("Did not pass system file name and/or dump filename\n%s", usage);
  }

  process_dump(sys_filename, dump_filename);

  return 0;
}