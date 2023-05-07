GIT_VERSION := $(shell git describe --abbrev=4 --dirty --always --tags)
ARC=/usr/local/amiga/bin/lha
CC=m68k-amigaos-gcc
HCC=gcc
#CFLAGS = -s -noixemul -Os -DREALBUILD# -DGITVERSION=\"$(GIT_VERSION)\"
CFLAGS = -O3 -noixemul -DUSECRC -Izlib-1.2.13/ -Lzlib-1.2.13/ 
HCFLAGS= -Wall -O3

all: sr sendami linser

sr: sr.c
	$(CC) $(CFLAGS) -o sr sr.c -lz
	ls -l sr
	cp sr ~/Documents/FS-UAE/Hard\ Drives/AmigaHD/t/

sendami: sendami.c
	$(HCC) $(HCFLAGS) -o sendami sendami.c -lz
	ls -l sendami

linser: linser.c
	$(HCC) $(HCFLAGS) -o linser linser.c -lz
	ls -l linser

release: sr
	$(ARC) a -o6 sr-$(GIT_VERSION).lha sr

clean:
	rm -f sr sendami sr*.lha *~ *.uaem foo_* linser
