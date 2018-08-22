CC = gcc
FLAGS = -Wall -Wextra -O0 -g

all: main

main: main.c
	$(CC) $(FLAGS) -o main main.c

clean:
	rm -rf *.o main
