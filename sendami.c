/* Copyright (C) 2017 by Alex */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zlib.h>

void writeHeader(char *str, unsigned int n) {
  char buf[1024];
  unsigned char filelen[4];
  unsigned char *s = filelen;

  unsigned char len = (unsigned char)strlen(str);
  assert(strlen(str) < 256);
  *s++ = (n & 0xFF000000) >> 24;
  *s++ = (n & 0x00FF0000) >> 16;
  *s++ = (n & 0x0000FF00) >> 8;
  *s++ = (n & 0x000000FF);
  sprintf(buf, "SRV1%c%s%c%c%c%c", len, str, filelen[0], filelen[1], filelen[2],
          filelen[3]);
  write(1, buf, len + 9);
}

int main(int argc, char *argv[]) {
  int file;
  unsigned char buf[1024];

  if (argc != 2) {
    fprintf(stderr, "Usage: %s file [>/serial-device/connected-to-amiga]\n",
            argv[0]);
    return 5;
  }

  if ((file = open(argv[1], O_RDONLY)) != -1) {
    int size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);
    fprintf(stderr, "size: %d\n", size);
    int bytesread = 0;
    int curr;
    writeHeader(argv[1], size);
    uLong crc = crc32(0L, Z_NULL, 0);
    while ((curr = read(file, buf, 1024))) {
      crc = crc32(crc, buf, curr);
      write(1, buf, curr);
      bytesread += curr;
    }
    fprintf(stderr, "Read/write %d bytes (%X).\n", bytesread,
            (unsigned int)crc);
  } else {
    fprintf(stderr, "%s error: Can't open file \"%s\"\n", argv[0], argv[1]);
    return 20;
  }
  return 0;
}
