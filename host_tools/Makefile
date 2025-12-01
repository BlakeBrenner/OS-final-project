CC ?= gcc
CFLAGS := -Wall -Wextra -std=c11

BINARIES := fork_demo shittyshell

all: $(BINARIES)

fork_demo: fork.c
	$(CC) $(CFLAGS) -o $@ $<

shittyshell: shittyshell.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINARIES)

.PHONY: all clean
