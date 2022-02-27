	incdir /opt/amiga/gcc6/m68k-amigaos/ndk-include/
	include <exec/macros.i>
        include <lvo/exec_lib.i>
        include <lvo/dos_lib.i>

        XREF    _DOSBase
	XDEF	_myprintf
	
MPIB = 512
_myprintf:
        movem.l d2/d3/a2/a3/a6,-(sp)
        movea.l 6*4(sp),a0              ; fmt string
        lea     7*4(sp),a1              ; pointer to variables
        lea     -MPIB(sp),sp            ; allocate stack space for produced string
        movea.l sp,a3                   ; a3 points to where RawDoFmt will write (on stack)
        lea.l   stuffChar(pc),a2
        move.l  $4.w,a6
        JSRLIB  RawDoFmt


        ; moveq   #0,d0
        ; lea.l   dosname,a1
        ; JSRLIB  OpenLibrary 
        ; move.l  d0,a6
        move.l  _DOSBase,a6
        JSRLIB  Output
        move.l  d0,d1                   ;file to write to (Output())

        move.l  a3,d2                   ;string start for Write
        move.l  a3,a1
.strlen tst.b   (a1)+
        bne.s   .strlen
        sub.l   a3,a1
        move.l  a1,d3                   ;length
        subq.l  #1,d3                   ;account for the fact that a1 has passed the \0 by one byte

        JSRLIB  Write

        lea     MPIB(sp),sp             ; deallocate buffer 
        movem.l (sp)+,d2/d3/a2/a3/a6
        rts

stuffChar:
        move.b  d0,(a3)+
        rts

	;
	; Simple version of the C "sprintf" function.  Assumes C-style
	; stack-based function conventions.
	;
	;   long eyecount;
	;   eyecount=2;
	;   sprintf(string,"%s have %ld eyes.","Fish",eyecount);
	;
	; would produce "Fish have 2 eyes." in the string buffer.
	;
		XDEF _mysprintf
_mysprintf:	; ( ostring, format, {values} )
		movem.l a2/a3/a6,-(sp)

		move.l	4*4(sp),a3       ;Get the output string pointer
		move.l	5*4(sp),a0       ;Get the FormatString pointer
		lea.l	6*4(sp),a1       ;Get the pointer to the DataStream
		lea.l	stuffChar(pc),a2
		move.l  $4.w,a6
		JSRLIB	RawDoFmt

		movem.l (sp)+,a2/a3/a6
		rts
dosname:	dc.b "dos.library",0
