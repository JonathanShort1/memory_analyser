CC = gcc
FLAGS = -Wall -Wextra -O0 -g
KER = $(shell uname -r)
INCLUDE_FLAG = -I /usr/src/linux-headers-$(KER)/include/linux/list.h

all: main 

main: main.c
	$(CC) $(FLAGS) -o main main.c

test: test.c
	$(CC) $(FLAGS) -o test test.c

clean:
	rm -rf *.o main test test-list
