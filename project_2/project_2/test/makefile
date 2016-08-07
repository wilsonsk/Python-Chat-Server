CC = gcc
CFLAGS = -Wall
CFLAGS += -D_GNU_SOURCE

chatClient:
	${CC} ftserver.c -o ftserver ${CFLAGS}

.PHONY: clean
clean:
	rm ftserver
