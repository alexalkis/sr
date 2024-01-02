#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <zlib.h>

enum retcodes { RET_OK = 0, RET_WARN = 5, RET_ERROR = 20 };

int getbaud(int n);
uint32_t fileCRC32(char *file);
int sendFile(int fd, char *file, int size);

int set_interface_attribs(int fd, int speed) {
  struct termios tty = {0};

  // memset(&tty, 0, sizeof tty);
  if (tcgetattr(fd, &tty) < 0) {
    printf("Error from tcgetattr: %s\n", strerror(errno));
    return -1;
  }

  cfsetospeed(&tty, (speed_t)speed);
  cfsetispeed(&tty, (speed_t)speed);

  tty.c_cflag |= CREAD | CLOCAL;  // Turn on READ & ignore ctrl lines (CLOCAL = 1)
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;     /* 8-bit characters */
  tty.c_cflag &= ~PARENB; /* no parity bit */
  tty.c_cflag &= ~CSTOPB; /* only need 1 stop bit */
  // tty.c_cflag |= CRTSCTS;  /* enable hardware flowcontrol */
  tty.c_cflag &= ~CRTSCTS; /* no flowcontrol */

  tty.c_lflag &= ~ICANON;
  tty.c_lflag &= ~ECHO;                    // Disable echo
  tty.c_lflag &= ~ECHOE;                   // Disable erasure
  tty.c_lflag &= ~ECHONL;                  // Disable new-line echo
  tty.c_lflag &= ~ISIG;                    // Disable interpretation of INTR, QUIT and SUSP
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);  // Turn off s/w flow ctrl
  tty.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);  // Disable any special handling of received bytes

  tty.c_oflag &= ~OPOST;  // Prevent special interpretation of output bytes (e.g. newline chars)
  tty.c_oflag &= ~ONLCR;  // Prevent conversion of newline to carriage return/line feed

  /* fetch bytes as they become available */
  tty.c_cc[VMIN] = 1;
  tty.c_cc[VTIME] = 0;  // 1;

  /* Make raw */
  // cfmakeraw(&tty);

  /* Flush Port, then applies attributes */
  tcflush(fd, TCIFLUSH);
  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    printf("Error from tcsetattr: %s\n", strerror(errno));
    return -1;
  }
  return 0;
}

void set_mincount(int fd, int mcount) {
  struct termios tty;

  if (tcgetattr(fd, &tty) < 0) {
    printf("Error tcgetattr: %s\n", strerror(errno));
    return;
  }

  tty.c_cc[VMIN] = mcount ? 1 : 0;
  tty.c_cc[VTIME] = 1; /* half second timer */

  if (tcsetattr(fd, TCSANOW, &tty) < 0) printf("Error tcsetattr: %s\n", strerror(errno));
}

int WriteSync(int fd, const void *buf, size_t length) {
  size_t byteswritten = write(fd, buf, length);
  if (byteswritten != length) {
    fprintf(stderr, "write() error: expecting %ld written bytes, got %ld. Errno: %d\n", length, byteswritten, errno);
    return RET_ERROR;
  }
  tcdrain(fd); /* wait for data to actually be transmitted */
  return RET_OK;
}

void help(char *prg) {}

#define DISPLAY_STRING
int main(int argc, char **argv) {
  char *portname = "/dev/ttyUSB0";
  int baud = B115200;
  int serial_fd;

  static const struct option opts[] = {
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {"device", required_argument, 0, 'd'},
      {"baud", required_argument, 0, 'b'},
      /* And so on */
      {0, 0, 0, 0} /* Sentiel */
  };
  int optidx;
  char c;

  /* <option> and a ':' means it's marked as required_argument, make sure to do that.
   * or optional_argument if it's optional.
   * You can pass NULL as the last argument if it's not needed.  */
  while ((c = getopt_long(argc, argv, "vhd:b:", opts, &optidx)) != -1) {
    switch (c) {
      case 'v':
        printf("%s alpha-release\n", argv[0]);
        return 0;
        break;
      case 'h':
        help(argv[0]);
        break;
      case 'd':
        portname = optarg;
        break;
      case 'b':
        baud = atol(optarg);
        baud = getbaud(baud);
        if (!baud) {
          fprintf(stderr, "%s error: Baud rate %s not found.\n", argv[0], optarg);
          return 20;
        }
        break;
      default:
        if (optopt == 'c')
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint(optopt))
          fprintf(stderr, "Unknown option -%c.\n", optopt);
        else
          fprintf(stderr, "Unknown option character '\\x%x'.\n", optopt);
        return 1;
    }
  }

  serial_fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (serial_fd < 0) {
    printf("Error opening %s: %s\n", portname, strerror(errno));
    return -1;
  }
  /*baudrate 115200, 8 bits, no parity, 1 stop bit */
  set_interface_attribs(serial_fd, baud);  // B115200);
  set_mincount(serial_fd, 0);              /* set to pure timed read */
  struct stat st;

  /* Loop through other arguments ("leftovers").  */
  while (optind < argc) {
    if (stat(argv[optind], &st)) {
      fprintf(stderr, "File %s not found.\n", argv[optind]);
    } else {
      /* whatever */;
      printf("%s %ld %08x\n", argv[optind], st.st_size, fileCRC32(argv[optind]));
      sendFile(serial_fd, argv[optind], st.st_size);
    }
    ++optind;
  }
  return 0;

  //   int counter = 0;
  //   /* simple noncanonical input */
  //   do {
  //     unsigned char buf[256];
  //     int rdlen;

  //     rdlen = read(serial_fd, buf, sizeof(buf) - 1);
  //     if (!rdlen) {
  //       printf("no data #%d\r", ++counter);
  //     }
  //     if (rdlen > 0) {
  // #ifdef DISPLAY_STRING
  //       buf[rdlen] = 0;
  //       printf("%s", buf);
  //       wlen = write(serial_fd, buf, rdlen);
  //       if (wlen != rdlen) {
  //         fprintf(stderr, "Error from write: %d, %d\n", wlen, errno);
  //       }
  //       tcdrain(serial_fd); /* delay for output */
  // #else                     /* display hex */
  //       unsigned char *p;
  //       printf("Read %d:", rdlen);
  //       for (p = buf; rdlen-- > 0; p++) printf(" %02x", *p);
  //       printf("\n");
  // #endif
  //     } else if (rdlen < 0) {
  //       fprintf(stderr, "Error from read: %d: %s\n", rdlen, strerror(errno));
  //     }
  //     /* repeat read to get full message */
  //   } while (1);
}

int getbaud(int n) {
  static int speeds[][2] = {
      /*B0,       0,*/
      B50,      50,      B75,      75,      B110,     110,     B134,     134,     B150,     150,     B200,     200,
      B300,     300,     B600,     600,     B1200,    1200,    B1800,    1800,    B2400,    2400,    B4800,    4800,
      B9600,    9600,    B19200,   19200,   B38400,   38400,   B57600,   57600,   B115200,  115200,  B230400,  230400,
      B460800,  460800,  B500000,  500000,  B576000,  576000,  B921600,  921600,  B1000000, 1000000, B1152000, 1152000,
      B1500000, 1500000, B2000000, 2000000, B2500000, 2500000, B3000000, 3000000, B3500000, 3500000, B4000000, 4000000};

  for (int i = 0; i < sizeof(speeds) / (2 * sizeof(int)); ++i) {
    // printf("%d\n", speeds[i][1]);
    if (speeds[i][1] == n) return speeds[i][0];
  }
  return 0;
}

uint32_t fileCRC32(char *file) {
  unsigned char buffer[256];
  uLong crc = crc32(0L, Z_NULL, 0);

  int fd = open(file, O_RDONLY);
  int len;
  while ((len = read(fd, buffer, sizeof buffer))) {
    crc = crc32(crc, buffer, len);
  }
  close(fd);
  return crc;
}

void writeHeader(char *str, int fd, unsigned int n) {
  char buf[1024];
  unsigned char filelen[4];
  unsigned char *s = filelen;

  unsigned char len = (unsigned char)strlen(str);
  assert(strlen(str) < 256);
  *s++ = (n & 0xFF000000) >> 24;
  *s++ = (n & 0x00FF0000) >> 16;
  *s++ = (n & 0x0000FF00) >> 8;
  *s++ = (n & 0x000000FF);
  sprintf(buf, "SRV1%c%s%c%c%c%c", len, str, filelen[0], filelen[1], filelen[2], filelen[3]);
  filelen[0] = len + 9;
  int w = write(fd, &filelen[0], 1);  // 1st byte will hold header's length
  assert(w == 1);
  w = write(fd, buf, len + 9);
  assert(w == len + 9);
}

int sendFile(int fd, char *file, int size) {
  unsigned char buffer[256];

  writeHeader(file, fd, size);
  int filefd = open(file, O_RDONLY);
  int len;
  int byteswritten = 0;
  while ((len = read(filefd, buffer, sizeof buffer))) {
    byteswritten += write(fd, buffer, len);
  }
  close(filefd);
  return byteswritten;
}