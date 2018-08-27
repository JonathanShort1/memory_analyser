CC = gcc
FLAGS = -Wall -Wextra -O0 -g
KER = $(shell uname -r)
INCLUDE_FLAG = -I /usr/src/linux-headers-$(KER)/include/linux/list.h

all: main

main: main.c
	$(CC) $(FLAGS) -o main main.c

clean:
	rm -rf *.o main
