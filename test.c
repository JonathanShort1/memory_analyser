#include <stdio.h>
#include <stdlib.h>

#define PAGE_MAP_MASK 0x0000FF8000000000
#define PDPT_MASK     0x0000007FC0000000
#define PDE_MASK      0x000000003FE00000
#define PTE_MASK      0x00000000001FF000
#define PAGE_OFF_MASK 0x0000000000000FFF

int main(void) {
  // 1111 1111 1111 1111 // 1111 1111 1-111 1111 10-00 0001 110-0 0000 1110 1011 0101 1000
  // unsigned long long vaddr = 0xffff88007c988358;
  unsigned long long vaddr = 0xFFFFFFFF81C0EB58;

  unsigned int pgt_offset = (vaddr & PAGE_MAP_MASK) >> 39;
  printf("pgt_offset: %d\n", pgt_offset);

  unsigned int pdpt_offset = (vaddr & PDPT_MASK) >> 30;
  printf("pdpt_offset: %d\n", pdpt_offset);

  unsigned int pde_offset = (vaddr & PDE_MASK) >> 21;
  printf("pde_offset: %d\n", pde_offset);

  unsigned int pte_offset = (vaddr & PTE_MASK) >> 12;
  printf("pte_offset: %d\n", pte_offset);

  unsigned int page_offset = vaddr & PAGE_OFF_MASK;
  printf("page_off: %d\n", page_offset);
}