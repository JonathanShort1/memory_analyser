#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <linux/sched.h>

#include "main.h"

#define PAGE_MAP_MASK 0x0000FF8000000000
#define PDPT_MASK     0x0000007FC0000000
#define PDE_MASK      0x000000003FE00000
#define PTE_MASK      0x00000000001FF000
#define PAGE_OFF_MASK 0x0000000000000FFF

void test_masks() {
  // 1111 1111 1111 1111 // 1111 1111 1-111 1111 10-00 0001 110-0 0000 1110 1011 0101 1000
  unsigned long long vaddr = 0xffff88007c988358;

  unsigned int pgt_offset = (vaddr & PAGE_MAP_MASK) >> 39;
  printf("pgt_offset: %x\n", pgt_offset);

  unsigned int pdpt_offset = (vaddr & PDPT_MASK) >> 30;
  printf("pdpt_offset: %x\n", pdpt_offset);

  unsigned int pde_offset = (vaddr & PDE_MASK) >> 21;
  printf("pde_offset: %x\n", pde_offset);

  unsigned int pte_offset = (vaddr & PTE_MASK) >> 12;
  printf("pte_offset: %x\n", pte_offset);

  unsigned int page_offset = vaddr & PAGE_OFF_MASK;
  printf("page_off: %x\n", page_offset);
}

void test_bit_shift() {
  unsigned long long y = 0x9678200000000000;
  printf("x: %llx\n", y);

  int byte;
  while ((byte = (y & 0xFF)) == 0) {
    y = y >> 8;
    printf("byte: %x, x: %llx\n", byte, y);
  }

  printf("byte: %x, x: %llx\n", byte, y);
}

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

void test_little_endian() {
  unsigned long long x = 0x12345678;
  unsigned long long y = to_little_endian(x);
  printf("x: %llx\ny: %llx\n", x, y);
}

int main(void) {
  test_masks();
  // printf("%d\n", sizeof(struct list_head));
}