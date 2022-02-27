/* Copyright (C) 2017 by Alex */
#include <clib/alib_protos.h>
#include <devices/serial.h>
#include <dos/dos.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USECRC
#include <zlib.h>
#endif

#ifdef LATTICE
int CXBRK(void) { return (0); }    /* Disable SAS CTRL/C handling */
int chkabort(void) { return (0); } /* really */
#endif

#define READ_BUFFER_SIZE 4096
unsigned char
    SerialReadBuffer[READ_BUFFER_SIZE]; /* Reserve SIZE bytes storage */
char fname[80];

#define isnum(x) (x >= '0' && x <= '9')

int nextargisnum(int i, char **argv, int argc, char c) {
  if ((i + 1) >= argc)
    return 0;
  ++i;

  if (argv[i][0] == '-') {
    printf("Numeric argument expected after the '-%lc' switch\n", c);
    return 0;
  }

  int j = 0;
  while (isnum(argv[i][j]))
    ++j;
  if (argv[i][j] != '\0') {
    printf("Numeric argument expected after the '-%lc' switch\n", c);
    return 0;
  }
  return 1;
}

void print_usage(char *prg) {
  printf("Usage: %s [-b baudrate] [-p priority]\n", prg);
}

typedef int bool;

static const char *eta_to_human_short(int secs, bool condensed) {
  static char buf[10]; /* 8 should be enough, but just in case */
  static int last = -1;
  const char *space = condensed ? "" : " ";

  /* Trivial optimization.  create_image can call us every 200 msecs
     (see bar_update) for fast downloads, but ETA will only change
     once per 900 msecs.  */
  if (secs == last)
    return buf;
  last = secs;

  if (secs < 100)
    sprintf(buf, "%ds", secs);
  else if (secs < 100 * 60)
    sprintf(buf, "%dm%s%ds", secs / 60, space, secs % 60);
  else if (secs < 48 * 3600)
    sprintf(buf, "%dh%s%dm", secs / 3600, space, (secs / 60) % 60);
  else if (secs < 100 * 86400)
    sprintf(buf, "%dd%s%dh", secs / 86400, space, (secs / 3600) % 24);
  else
    /* even (2^31-1)/86400 doesn't overflow BUF. */
    sprintf(buf, "%dd", secs / 86400);

  return buf;
}

int main(int argc, char **argv) {
  struct MsgPort *SerialMP;  /* pointer to our message port */
  struct IOExtSer *SerialIO; /* pointer to our IORequest */
  ULONG WaitMask, Temp;
  int filesize;
  int priority = 0;
  BPTR f = 0;
  struct DateStamp startTime, endTime;
#ifdef USECRC
  uLong crc;
#endif
  int baudrate = 19600;
  int argi = 1;

  while (argv[argi][0] == '-') {
    switch (argv[argi][1]) {
    case 'b':
      if (nextargisnum(argi, argv, argc, 'b')) {
        baudrate = atoi(argv[argi + 1]);
        ++argi;
      } else {
        return 5;
      }
      break;
    case 'p':
      if (nextargisnum(argi, argv, argc, 'p')) {
        priority = atoi(argv[argi + 1]);
        ++argi;
      } else {
        return 5;
      }
      break;
    default:
      print_usage(argv[0]);
      return 5;
    }
    ++argi;
    if (argi >= argc)
      break;
  }

  SetTaskPri(FindTask(NULL), priority);
  /* Create the message port */
  if (SerialMP = CreatePort(NULL, 0)) {
    /* Create the IORequest */
    if (SerialIO =
            (struct IOExtSer *)CreateExtIO(SerialMP, sizeof(struct IOExtSer))) {
      /* Open the serial device */
      if (OpenDevice(SERIALNAME, 0, (struct IORequest *)SerialIO, 0L)) {
        /* Inform user that it could not be opened */
        printf("Error: %s did not open\n", SERIALNAME);
      } else {
        WaitMask =
            SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F | 1L << SerialMP->mp_SigBit;

        /* Device is open */ /* DoIO - demonstrates synchronous */
        SerialIO->IOSer.io_Command =
            SDCMD_QUERY; /* device use, returns error or 0. */
        if (DoIO((struct IORequest *)SerialIO))
          printf("Query  failed. Error - %ld\n", SerialIO->IOSer.io_Error);
        else {
          /* Print serial device status - see include file for meaning */
          /* Note that with DoIO, the Wait and GetMsg are done by Exec */
          printf("Serial device status: $%lx\nUnread chars: %ld Baud: %ld\n\n",
                 SerialIO->io_Status, SerialIO->IOSer.io_Actual,
                 SerialIO->io_Baud);
        }

        SerialIO->io_StopBits = 1;
        SerialIO->io_RBufLen = 4096;
        SerialIO->io_ReadLen = SerialIO->io_WriteLen = 8;
        SerialIO->io_SerFlags &= ~SERF_PARTY_ON; /* set parity off */
        SerialIO->io_SerFlags |= SERF_XDISABLED; /* set xON/xOFF disabled */
        SerialIO->io_Baud = baudrate;            /* set baud */
        SerialIO->IOSer.io_Command = SDCMD_SETPARAMS; /* Set params command */
        if (DoIO((struct IORequest *)SerialIO))
          printf("Error setting parameters!\n");

        SerialIO->IOSer.io_Command = CMD_CLEAR;
        DoIO((struct IORequest *)SerialIO);

        if (SerialIO->IOSer.io_Error) {
          printf("Clear error: #%ld\n", (int)SerialIO->IOSer.io_Error);
        }

        SerialIO->IOSer.io_Command =
            SDCMD_QUERY; /* device use, returns error or 0. */
        if (DoIO((struct IORequest *)SerialIO))
          printf("Query  failed. Error - %ld\n", SerialIO->IOSer.io_Error);
        else {
          /* Print serial device status - see include file for meaning */
          /* Note that with DoIO, the Wait and GetMsg are done by Exec */
          printf("Serial device status: $%lx\nUnread chars: %ld Baud: %ld\n\n",
                 SerialIO->io_Status, SerialIO->IOSer.io_Actual,
                 SerialIO->io_Baud);
        }

        SerialIO->IOSer.io_Command = CMD_READ;
        SerialIO->IOSer.io_Length = READ_BUFFER_SIZE;
        SerialIO->IOSer.io_Data = (APTR)&SerialReadBuffer[0];
        SendIO((struct IORequest *)SerialIO);

        int sum = 0;
        int header = 0;
        int abort = 0;
        printf("ctrl-c aborts.  Send file to serial now...\n");
        while (!abort) {
          Temp = Wait(WaitMask);

          if (SIGBREAKF_CTRL_C & Temp) {
            if (f)
              Close(f);
            break;
          }

          if (CheckIO((struct IORequest *)SerialIO)) {
            /* If request is complete... */
            WaitIO((struct IORequest *)SerialIO);
            /* clean up and remove reply */

            if (!header) {
              DateStamp(&startTime);
              int pos = 0;
              header = 1;
              /* show first 16 bytes in hex for debugging pupropes
                 remove at release */
              for (pos = 0; pos < 16; ++pos) {
                printf("%02lX ", (unsigned int)SerialReadBuffer[pos]);
              }
              printf("\n");
              if (strncmp(SerialReadBuffer, "SRV1", 4)) {
                printf(
                    "Format not known, please use sendami from pc side...\n");
                abort = 1;
                continue;
              }

              int filenamelen = (unsigned char)SerialReadBuffer[4];
              strcpy(fname, "foo_"); // this is for debugging, remove at release
              strncat(fname, &SerialReadBuffer[5], filenamelen);
              f = Open(fname, MODE_NEWFILE);
              if (!f) {
                printf("Hmm, couldn't write file...aborting\n");
                break;
              }
              // printf("Filename length: %ld\n", filenamelen);
              unsigned char *fs = &SerialReadBuffer[5 + filenamelen];
              // for (pos=0; pos < 4; ++pos) printf("%lx\n", (int)fs[pos]);
              filesize = (((unsigned int)fs[0]) << 24) |
                         (((unsigned int)fs[1]) << 16) |
                         (((unsigned int)fs[2]) << 8) | (((unsigned int)fs[3]));
              printf("Filesize incoming: %ld\n", filesize);
              sum = SerialIO->IOSer.io_Actual - (5 + filenamelen + 4);
#ifdef USECRC
              crc = crc32(0L, Z_NULL, 0);
              crc = crc32(crc, &SerialReadBuffer[5 + filenamelen + 4], sum);
#endif
              Write(f, &SerialReadBuffer[5 + filenamelen + 4], sum);
            } else {
              sum += SerialIO->IOSer.io_Actual;
#ifdef USECRC
              crc = crc32(crc, SerialReadBuffer, SerialIO->IOSer.io_Actual);
#endif
              Write(f, SerialReadBuffer, SerialIO->IOSer.io_Actual);
            }

            DateStamp(&endTime);
            int ticks = (endTime.ds_Minute - startTime.ds_Minute) * 60 *
                            TICKS_PER_SECOND +
                        (endTime.ds_Tick - startTime.ds_Tick);
            int seconds = ticks / TICKS_PER_SECOND;
            int rem = filesize * seconds / sum - seconds;
            // printf("\nRem = %ld Ticks=%ld seconds = %ld\n", rem, ticks,
            // seconds);
            printf("%ld/%ld bytes [%ld] %s     \r", sum, filesize,
                   sum * TICKS_PER_SECOND / ticks, eta_to_human_short(rem, 0));
            if (sum >= filesize) {
              printf("\n%ld bytes received in %ld seconds (%ld ticks).\n",
                     filesize, seconds, ticks);
              Close(f);
              break;
            } else {
              fflush(stdout);
            }

            SerialIO->IOSer.io_Length = -1;
            SerialIO->IOSer.io_Data = (APTR) "Save the whales! ";
            SerialIO->IOSer.io_Command = CMD_WRITE;
            DoIO((struct IORequest *)SerialIO);

            SerialIO->IOSer.io_Command = CMD_READ;
            SerialIO->IOSer.io_Length = ((filesize - sum) < READ_BUFFER_SIZE)
                                            ? filesize - sum
                                            : READ_BUFFER_SIZE;
            SerialIO->IOSer.io_Data = (APTR)&SerialReadBuffer[0];
            SendIO((struct IORequest *)SerialIO);
          }
        }

        if (sum != filesize) {
          if (!(SIGBREAKF_CTRL_C & Temp))
            printf("Something went wrong\n");
        } else {
#ifdef USECRC
          printf("\n(%lx)\n", crc);
#else
          printf("\n");
#endif
        }
        AbortIO((struct IORequest *)SerialIO);
        /* Ask device to abort request, if pending */
        WaitIO((struct IORequest *)SerialIO);
        /* Wait for abort, then clean up */

        /* Close the serial device */
        CloseDevice((struct IORequest *)SerialIO);
      }
      /* Delete the IORequest */
      DeleteExtIO((struct IORequest *)SerialIO);
    } else {
      /* Inform user that the IORequest could be created */
      printf("Error: Could create IORequest\n");
    }
    /* Delete the message port */
    DeletePort(SerialMP);
  } else {
    /* Inform user that the message port could not be created */
    printf("Error: Could not create message port\n");
  }
}
