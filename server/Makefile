ifneq ($(CROSS_COMPILE),)
  CC = $(CROSS_COMPILE)gcc
else
  CC = gcc
endif

all: aesdsocket

default: aesdsocket

clean:
	rm -f aesdsocket *.o

aesdsocket: aesdsocket.o
	$(CC) $^ -o $@

aesdsocket.o:
	$(CC) aesdsocket.c -Wall -g -c -o $@

