GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
ARC=/usr/local/amiga/bin/lha
CC=m68k-amigaos-gcc
HCC=gcc
CFLAGS = -s -mcrt=nix13 -Os # -DGITVERSION=\"$(GIT_VERSION)\"
HCFLAGS= -Wall

all: sr

sr: sr.c
	$(CC) $(CFLAGS) -o sr sr.c -lm
	ls -l sr

release: sr
	$(ARC) a -o6 sr-$(GIT_VERSION).lha sr

clean:
	rm -f sr sendami sr*.lha *~ *.uaem foo_*
