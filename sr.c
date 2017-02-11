/* Copyright (C) 2017 by Alex */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <dos/dos.h>
#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

#ifdef USECRC
#include <zlib.h>
#endif

#ifdef LATTICE
int CXBRK(void) { return(0); }  /* Disable SAS CTRL/C handling */
int chkabort(void) { return(0); }  /* really */
#endif

#define READ_BUFFER_SIZE 4096
unsigned char SerialReadBuffer[READ_BUFFER_SIZE]; /* Reserve SIZE bytes storage */
char fname[80];

#define isnum(x) (x>='0' && x<='9')

int nextargisnum(int i, char **argv, int argc, char c) {
  if ((i+1) >= argc) return 0;
  ++i;

  if (argv[i][0] == '-') {
    Printf("Numeric argument expected after the '-%lc' switch\n", c);
    return 0;
  }

  int j=0;
  while(isnum(argv[i][j]))
        ++j;
  if (argv[i][j]!='\0') {
    Printf("Numeric argument expected after the '-%lc' switch\n", c);
    return 0;
  }
  return 1;
}


void print_usage(char *prg) {
  Printf("Usage: %s [-b baudrate]\n", prg);
}

int main(int argc, char **argv) {
  struct MsgPort *SerialMP;       /* pointer to our message port */
  struct IOExtSer *SerialIO;      /* pointer to our IORequest */
  ULONG WaitMask, Temp;
  int filesize;
  BPTR f = 0;
  struct DateStamp startTime, endTime;
  #ifdef USECRC
  uLong crc;
  #endif
  int baudrate = 31250;
  int argi = 1;

  while ( argv[argi][0] == '-' ) {
    switch (argv[argi][1]) {
      case 'b':
        if (nextargisnum(argi, argv, argc, 'b')) {
          baudrate = atoi(argv[argi+1]);
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

  /* Create the message port */
  if (SerialMP=CreateMsgPort()) {
    /* Create the IORequest */
    if (SerialIO = (struct IOExtSer *) CreateExtIO(SerialMP, sizeof(struct IOExtSer))) {
      /* Open the serial device */
      if (OpenDevice(SERIALNAME, 0, (struct IORequest *)SerialIO, 0L)) {
        /* Inform user that it could not be opened */
        Printf("Error: %s did not open\n", SERIALNAME);
      } else {
        WaitMask = SIGBREAKF_CTRL_C|
                   SIGBREAKF_CTRL_F|
                   1L << SerialMP->mp_SigBit;


        /* Device is open */                         /* DoIO - demonstrates synchronous */
        SerialIO->IOSer.io_Command  = SDCMD_QUERY;   /* device use, returns error or 0. */
        if (DoIO((struct IORequest *)SerialIO))
          Printf("Query  failed. Error - %ld\n",SerialIO->IOSer.io_Error);
        else {
          /* Print serial device status - see include file for meaning */
          /* Note that with DoIO, the Wait and GetMsg are done by Exec */
          Printf("Serial device status: $%lx\nUnread chars: %ld Baud: %ld\n\n",
                 SerialIO->io_Status,
                 SerialIO->IOSer.io_Actual,
                 SerialIO->io_Baud);
        }
        
        SerialIO->io_StopBits = 1;
        SerialIO->io_RBufLen = 4096;
        SerialIO->io_ReadLen = SerialIO->io_WriteLen = 8;
        SerialIO->io_SerFlags      &= ~SERF_PARTY_ON; /* set parity off */
        SerialIO->io_SerFlags      |= SERF_XDISABLED; /* set xON/xOFF disabled */
        SerialIO->io_Baud           = baudrate;           /* set baud */
        SerialIO->IOSer.io_Command  = SDCMD_SETPARAMS;/* Set params command */
        if (DoIO((struct IORequest *)SerialIO))
          Printf("Error setting parameters!\n");

        SerialIO->IOSer.io_Command = CMD_CLEAR;
        DoIO((struct IORequest *)SerialIO);

        if (SerialIO->IOSer.io_Error) {
          Printf("Clear error: #%ld\n", (int)SerialIO->IOSer.io_Error);
        }



        /* Device is open */                         /* DoIO - demonstrates synchronous */
        SerialIO->IOSer.io_Command  = SDCMD_QUERY;   /* device use, returns error or 0. */
        if (DoIO((struct IORequest *)SerialIO))
          Printf("Query  failed. Error - %ld\n",SerialIO->IOSer.io_Error);
        else {
          /* Print serial device status - see include file for meaning */
          /* Note that with DoIO, the Wait and GetMsg are done by Exec */
          Printf("Serial device status: $%lx\nUnread chars: %ld Baud: %ld\n\n",
                 SerialIO->io_Status,
                 SerialIO->IOSer.io_Actual,
                 SerialIO->io_Baud);
        }
        
        

        SerialIO->IOSer.io_Command  = CMD_READ;
        SerialIO->IOSer.io_Length   = READ_BUFFER_SIZE;
        SerialIO->IOSer.io_Data     = (APTR)&SerialReadBuffer[0];
        SendIO((struct IORequest *)SerialIO);

        int sum = 0;
        int header = 0;
        int abort = 0;
        Printf("ctrl-c aborts.  Send file to serial now...\n");
        while (!abort) {
          Temp = Wait(WaitMask);

          if (SIGBREAKF_CTRL_C & Temp) {
            if (f) Close(f);
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
              for (pos=0; pos < 16; ++pos) {
                Printf("%02lX ", (unsigned int)SerialReadBuffer[pos]);
              }
              Printf("\n");
              if (strncmp(SerialReadBuffer, "SRV1", 4)) {
                Printf("Format not known, please use sendami from pc side...\n");
                abort = 1;
                continue;
              }

              int filenamelen = (unsigned char) SerialReadBuffer[4];
              strcpy(fname,"foo_");  // this is for debugging, remove at release
              strncat(fname, &SerialReadBuffer[5], filenamelen);
              f = Open(fname, MODE_NEWFILE);
              if (!f) {
                Printf("Hmm, couldn't write file...aborting\n");
                break;
              }
              Printf("Filename length: %ld\n", filenamelen);
              unsigned char *fs = &SerialReadBuffer[5+filenamelen];
              for (pos=0; pos < 4; ++pos) Printf("%lx\n", (int)fs[pos]);
              filesize = (((unsigned int)fs[0]) << 24) |
                  (((unsigned int)fs[1]) << 16) |
                  (((unsigned int)fs[2]) << 8) |
                  (((unsigned int)fs[3]));
              Printf("Filesize incoming: %ld\n", filesize);
              sum = SerialIO->IOSer.io_Actual-(5+filenamelen+4);
              #ifdef USECRC
              crc = crc32(0L, Z_NULL, 0);
              crc = crc32(crc, &SerialReadBuffer[5+filenamelen+4], sum);
              #endif
              Write(f, &SerialReadBuffer[5+filenamelen+4], sum);
           } else {
              sum += SerialIO->IOSer.io_Actual;
              #ifdef USECRC
              crc = crc32(crc, SerialReadBuffer, SerialIO->IOSer.io_Actual);
              #endif
              Write(f, SerialReadBuffer, SerialIO->IOSer.io_Actual);
           }

            if ((filesize-sum) < READ_BUFFER_SIZE)
              SerialIO->IOSer.io_Length = filesize-sum;

            DateStamp(&endTime);
            int ticks = (endTime.ds_Minute-startTime.ds_Minute)*60*TICKS_PER_SECOND+(endTime.ds_Tick-startTime.ds_Tick);
            Printf("%ld/%ld bytes [%ld]\r", sum, filesize, sum*TICKS_PER_SECOND/ticks);
            if (sum >= filesize) {
              Close(f);
              break;
            }
            SendIO((struct IORequest *)SerialIO);
          }
        }

        if (sum != filesize) {
          if (!(SIGBREAKF_CTRL_C & Temp))
            Printf("Something went wrong\n");
        } else {
          #ifdef USECRC
          Printf("\n(%lx)\n",crc);
          #else
          Printf("\n");
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
      Printf("Error: Could create IORequest\n");
    }
    /* Delete the message port */
    DeleteMsgPort(SerialMP);
  } else {
    /* Inform user that the message port could not be created */
    Printf("Error: Could not create message port\n");
  }
}

