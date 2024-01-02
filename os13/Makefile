GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
ARC=/usr/local/amiga/bin/lha
CC=m68k-amigaos-gcc
HCC=gcc
CFLAGS = -s -mcrt=nix13 -Os -DREALBUILD -DUSECRC -I../zlib-1.2.13/ -L../zlib-1.2.13/# -DGITVERSION=\"$(GIT_VERSION)\"
HCFLAGS= -Wall

all: sr

sr: sr.c
	$(CC) $(CFLAGS) -o sr sr.c -lm -lz
	ls -l sr

release: sr
	$(ARC) a -o6 sr-$(GIT_VERSION).lha sr

clean:
	rm -f sr sendami sr*.lha *~ *.uaem foo_*
