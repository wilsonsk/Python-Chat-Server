CC = gcc
CFLAGS = -Wall
CFLAGS += -std=c99
CFLAGS += -D_GNU_SOURCE

chatClient:
	${CC} chatClient.c -o chatClient ${CFLAGS}

.PHONY: clean
clean:
	rm chatClient
