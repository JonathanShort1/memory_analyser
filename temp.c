/**
 * This function will return the value in a page table/map/directory
 * (program will die on failure)
 * @params fd - file descriptor of dump file
 * @params base_addr - base address of page table
 * @params index - the index within the page table
 * @returns - the value at the page table
*/
unsigned long long retreive_page_table_addr(int fd, LHdr_list *list, 
      unsigned long long base_addr, unsigned int index, int pte_flag) {

  printf("retreive_page_table_addr - base_addr: %llx\n", base_addr);
  LHdr *l = get_correct_header_and_seek(fd, list, base_addr);
  
  if (lseek64(fd, (base_addr + (8 * index)) - l->s_addr, SEEK_CUR) == -1) {
    _die("retreive_page_table_addr - Unable to seek to index %llx of table: %llx", index, base_addr);
  }

  unsigned long long paddr_next = 0;
  if (pte_flag) {
    if (read(fd, &paddr_next,sizeof(unsigned int)) == -1) {
      _die("unable to read in address at %llcx", base_addr + index);
     }
  } else {
     if (read(fd, &paddr_next,sizeof(unsigned long long)) == -1) {
      _die("unable to read in value at %llcx", base_addr + index);
     }
  }
  return paddr_next;
}

/**
 * This function translates a virtual address to a physical address
 * (Cannot be used if static offset and PA_MAX is not set - use get_lime_headers)
 * (only works for nokaslr so far)
 * SAFE TO SEEK
 * @params fd - file descriptor
 * @params list - list of lime headers
 * @params vaddr - the virtual address to be translated
 * @returns paddr - the physical address or -1 on failure 
*/
unsigned long long paddr_translation(int fd, LHdr_list *list, const unsigned long long vaddr) {
  if (!STATIC_SHIFT || !PA_MAX){
    _die("paddr_translation - Must call get_lime_headers before using this function");
  }

  /* Is address in directly mapped region */
  if (STATIC_SHIFT < vaddr && vaddr <= STATIC_SHIFT + PA_MAX) {
    return vaddr - STATIC_SHIFT;
  } else {
    /* Use page tables */
    unsigned int pa_pgt_offset = (vaddr & PAGE_MAP_MASK) >> 39;
    unsigned long long pa_pdpt = retreive_page_table_addr(fd, list, PGT_PADDR, pa_pgt_offset, PAGE_TABLE_ENTRY);
    unsigned int pa_pdpt_offset = (vaddr & PDPT_MASK) >> 30; 
    unsigned long long pa_pde = retreive_page_table_addr(fd, list, pa_pdpt, pa_pdpt_offset, PAGE_TABLE_ENTRY);
    unsigned int pa_pde_offset = (vaddr & PDE_MASK) >> 21;
    unsigned long long pa_pte = retreive_page_table_addr(fd, list, pa_pde, pa_pde_offset, PAGE_TABLE_ENTRY);
    unsigned int pa_pte_offset = (vaddr & PTE_MASK) >> 12;
    unsigned long long pa_page = retreive_page_table_addr(fd, list, pa_pte, pa_pte_offset, PAGE_ADDRESS);
    unsigned int page_offset = vaddr & PAGE_OFF_MASK;
    printf("pa_pgt_offset: %x\n", pa_pgt_offset);
    printf("pa_pdpt: %llx\n", pa_pdpt);
    printf("pa_pde: %llx\n", pa_pde);
    printf("pa_pte: %llx\n", pa_pte);
    printf("pa_page: %llx\n", pa_page);
    return pa_page + (8 * page_offset);
  }
  return -1;
}

/**
 * This function seeks to the base of the given task
 * Does not restore seek offset
 * @params fd - file descriptor of dump file
 * @params list - list of lime headers
 * @params vaddr - virtual address of base 
*/
void seek_to_base_of_task(int fd, LHdr_list *list, unsigned long long vaddr) {
  unsigned long long paddr = paddr_translation(fd, list, vaddr);

  if (paddr != -1ULL) {
    LHdr *header = get_correct_header_and_seek(fd, list, paddr);
      
    if (lseek64(fd, paddr - header->s_addr, SEEK_CUR) == -1) {
      _die("seek_to_base_of_task - Could not seek to base of task");
    }
  } else {
    _debug("DBUG: couldn't find virtual address");
  }
}

/**
 * This function prints out all the processes in the task_struct list starting at
 * the task_struct passed
 * @params ts - the task_struct to be pased first
*/
void print_process_list(int fd, LHdr_list *list, struct task_struct *init) {
  printf("\n\ncomm: %s - pid: %d - child: %p\n", init->comm, init->pid, init->tasks.next);

  seek_to_base_of_task(fd, list, (unsigned long long) init->tasks.next);

  struct task_struct *next = task_struct_init();
  get_task_attr(fd, next, comm_offset, TASK_COMM_LEN, TASK_COMM_ID);

  /*struct list_head *pos;
  struct task_struct *curr;
  list_for_each(pos, &init->tasks) {
    curr = list_entry(pos, struct task_struct, tasks);
    printf("comm: %s", curr->comm);
  }*/
  printf("task: %s\n", next->comm);
}