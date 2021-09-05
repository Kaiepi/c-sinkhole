CC?=cc
RM?=rm
INSTALL?=install

PREFIX?=/usr/local

.PHONY: all clean install

all: sinkhole

sinkhole: sinkhole.c
	$(CC) -lcurses sinkhole.c -o sinkhole

clean:
	$(RM) -f sinkhole

install:
	$(INSTALL) sinkhole $(PREFIX)/bin
