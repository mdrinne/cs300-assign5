all: fifo lru

fifo :
	gcc -g fifo.c -o fifo -Wall -Wextra

lru :
	gcc -g lru.c -o lru -Wall -Wextra

clean :
	rm -f fifo lru
