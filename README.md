# sr
Serial Receive (sr) is a small utility for Amiga that will help receiving files over the serial port with a null-modem cable.

## Description ##

 
So, you have your amiga and a pc and you wonder how to transfer files from pc to amiga.


This page <http://www.boomerangsworld.de/cms/vc/amiga_transfer.html> is nice to give you a start.
It describes the necessary port configurations both on pc and amiga, and finally a small program (a few lines of source code) which is in AmigaBasic for pre-2.0 amigas and Arexx for >=2.0 amigas that will help you transfer a file from pc to amiga.

The simplest way would be to go on amiga side and type in a shell

> type ser: >file

and on the pc side (assuming linux) 

> cat file >/dev/ttyS0

One unfortunate thing is that amiga's

> type

command doesn't work for binary files after Workbench1.3.  For text files it works no matter the version of the workbench.

That is the reason you either need to write the following AmigaBasic
```AmigaBasic
' Written by Wolfgang Stoeggl (1998)
INPUT "filename? ",file$
INPUT "size (bytes)? ", size&
PRINT "Now send the file!"
OPEN "ser:" FOR INPUT AS #1
OPEN file$ FOR OUTPUT AS #2
n% = 1024
WHILE LOF(2) < size&
 diff&=size&-LOF(2)
 IF diff&<1024 THEN n%=diff& 
 t$=INPUT$(n%,#1):PRINT #2,t$; 
 LOCATE 5,1
 PRINT LOF(2)
WEND
PRINT "Received file: ",file$
PRINT "filelength =", LOF(2),"bytes"   
CLOSE
END
```
or the following Arexx

`/* receive.rexx
** Written by Wolfgang Stoeggl (1998, 2004) */
say 'Filename?'; pull file
say 'Bytes?'; pull size
say 'Now send the file!'
open('1','ser:')
open('2',file,'W')
n = 1024
lof = 0
do while lof < size
 lof = seek('2', 0, E)
 diff = size-lof
 if diff < 1024 then n = diff
 t = readch('1', n)
 writech('2', t)
 say lof || '0b'x
end
say 'Received file: 'file''
say 'Filelength = 'lof' bytes'
close('1'); close('2')
exit`
