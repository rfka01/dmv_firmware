;=====================================================================
; KEYCLICK.COM  -  switch the key click of the NCR Decision Mate V
;                  on or off
;
;   KEYCLICK            show the current setting
;   KEYCLICK ON         key click on
;   KEYCLICK OFF        key click off
;   KEYCLICK LOW        just a faint tick
;
; Exit codes (ERRORLEVEL):
;   0  done            1  wrong parameter
;   2  firmware without key click switch
;   3  keyboard controller does not answer
;   4  firmware without LOW (first patch version); click switched on
;
; Requires MS-DOS 2.0 or later on the DMV with 8088/V20 card
; (K230/K231/K235) and the patched mainboard keyboard controller
; (8741, ROM 32678 with key click patch).
;
; Hardware:
;   The click comes from the speaker on the mainboard, driven by P2.0
;   of the 8741 keyboard controller (not by the keyboard). The original
;   firmware plays it unconditionally whenever it passes a key code to
;   the host (about 870 Hz, 20 ms). The patched firmware adds three
;   commands:
;     02h  key click off           03h  key click on
;     04h  faint tick: one 0.5 ms pulse instead of a tone
;   and reports in the user bits of its status register:
;     bit 6  patch present (set after the self test, i.e. after boot)
;     bit 5  key click off         bit 4  tick (LOW)
;   40h data, 41h command/status, status bit 1 = IBF (input buffer
;   full). The setting lives in the 8741 RAM; every reset (self test)
;   switches the click on again.
;
;   The original firmware must never get commands 02h..05h: it then
;   loops in its command dispatcher and stops scanning the keyboard
;   until the next command byte. KEYCLICK therefore checks bit 6 first
;   and does nothing without the patch. The first patch version knows
;   02h/03h but not 04h; if LOW is not confirmed, KEYCLICK sends 03h,
;   which ends the loop, and reports that LOW is missing.
;
; Build with MASM 5.10:  MASM KEYCLICK; LINK KEYCLICK;
;                        EXE2BIN KEYCLICK.EXE KEYCLICK.COM
;=====================================================================

KB_STAT EQU     41h             ; 8741 status (read) / command (write)
ST_IBF  EQU     02h             ; input buffer full
ST_LOW  EQU     10h             ; tick (LOW)
ST_OFF  EQU     20h             ; key click off
ST_MODE EQU     30h
ST_PATCH EQU    40h             ; key click patch present
CMD_OFF EQU     02h
CMD_ON  EQU     03h
CMD_LOW EQU     04h

CSEG    SEGMENT PARA PUBLIC 'CODE'
        ASSUME  CS:CSEG,DS:CSEG,ES:CSEG,SS:CSEG
        ORG     100h

START:  jmp     main

;=====================================================================
;  Data
;=====================================================================

mode    db      0                       ; 0 show, CMD_OFF, CMD_ON, CMD_LOW
param   db      4 dup (0)               ; parameter, upper case

keytab  db      'ON',0,0,  CMD_ON
        db      'OFF',0,   CMD_OFF
        db      'LOW',0,   CMD_LOW
        db      0

msgon   db      'Tastaturklick ist AN',13,10,'$'
msgoff  db      'Tastaturklick ist AUS',13,10,'$'
msglow  db      'Tastaturklick ist LOW',13,10,'$'
msgnolo db      'Diese Version des Klick-Patches kennt LOW noch nicht.',13,10
        db      'Der Klick bleibt AN. Bitte das neue ROM einsetzen.',13,10,'$'
msgnop  db      'Der Tastatur-Controller (8741 auf der Hauptplatine) hat die',13,10
        db      'Originalfirmware ohne Klick-Schalter. KEYCLICK braucht das',13,10
        db      'gepatchte ROM 32678 (DMV_MB_8741_32678_KEYCLICK.BIN).',13,10,'$'
msgtime db      'Der Tastatur-Controller antwortet nicht.',13,10,'$'
msguse  db      'KEYCLICK - Tastaturklick der NCR Decision Mate V',13,10
        db      13,10
        db      '  KEYCLICK        zeigt die Einstellung',13,10
        db      '  KEYCLICK ON     Tastaturklick an',13,10
        db      '  KEYCLICK OFF    Tastaturklick aus',13,10
        db      '  KEYCLICK LOW    nur ein leiser Knack',13,10
        db      13,10
        db      'Nach jedem Reset ist der Klick wieder an.',13,10,'$'

;=====================================================================
;  Code
;=====================================================================

main:   cld
        mov     si,81h                  ; command tail
        call    skipsp
        cmp     al,13
        je      check                   ; no parameter: show
        mov     di,offset param         ; copy up to 4 characters
        mov     cx,4
cpy:    cmp     al,' '
        je      cpyend
        cmp     al,9
        je      cpyend
        cmp     al,13
        je      cpyend
        test    cx,cx
        jnz     cpyok
        jmp     usage                   ; word too long
cpyok:
        cmp     al,'a'
        jb      cpyup
        cmp     al,'z'
        ja      cpyup
        sub     al,20h
cpyup:  stosb
        dec     cx
        lodsb
        jmp     cpy
cpyend: dec     si                      ; nothing may follow
        call    skipsp
        cmp     al,13
        jne     usage
        mov     bx,offset keytab        ; look the word up
look:   cmp     byte ptr [bx],0
        je      usage
        mov     ax,word ptr [bx]
        cmp     ax,word ptr param
        jne     lnext
        mov     ax,word ptr [bx+2]
        cmp     ax,word ptr param+2
        jne     lnext
        mov     al,[bx+4]
        mov     mode,al
        jmp     short check
lnext:  add     bx,5
        jmp     look

check:  in      al,KB_STAT
        test    al,ST_PATCH
        jz      nopatch
        cmp     mode,0
        je      show

        call    sendcmd                 ; CF = timeout
        jc      timeout
        call    waitst                  ; wait until the status shows it
        jnc     show
        cmp     mode,CMD_LOW
        jne     timeout
        mov     mode,CMD_ON             ; first patch version: 03h ends
        call    sendcmd                 ; its dispatcher loop
        jc      timeout
        call    waitst
        jc      timeout
        mov     dx,offset msgnolo
        mov     ah,9
        int     21h
        mov     al,4
        jmp     short exit

show:   in      al,KB_STAT
        mov     dx,offset msgoff
        test    al,ST_OFF
        jnz     showit
        mov     dx,offset msglow
        test    al,ST_LOW
        jnz     showit
        mov     dx,offset msgon
showit: mov     ah,9
        int     21h
        mov     al,0
        jmp     short exit

usage:  mov     dx,offset msguse
        mov     ah,9
        int     21h
        mov     al,1
        jmp     short exit

nopatch:
        mov     dx,offset msgnop
        mov     ah,9
        int     21h
        mov     al,2
        jmp     short exit

timeout:
        mov     dx,offset msgtime
        mov     ah,9
        int     21h
        mov     al,3

exit:   mov     ah,4Ch
        int     21h

;---------------------------------------------------------------------
; skipsp: next character after blanks and tabs -> AL, SI behind it
;---------------------------------------------------------------------
skipsp: lodsb
        cmp     al,' '
        je      skipsp
        cmp     al,9
        je      skipsp
        ret

;---------------------------------------------------------------------
; sendcmd: write command 'mode' to the 8741 as soon as its input
; buffer is empty. The final test and the OUT run with interrupts off
; so that the BIOS cannot write to the 8741 in between.
; The 8741 may be busy for up to about 0.6 s (bell), so wait long.
; Out: CF set on timeout.
;---------------------------------------------------------------------
sendcmd:
        mov     bx,20                   ; about 20 x 65536 polls
        xor     cx,cx
sc_wait:
        in      al,KB_STAT
        test    al,ST_IBF
        jnz     sc_next
        cli
        in      al,KB_STAT
        test    al,ST_IBF
        jnz     sc_busy
        mov     al,mode
        out     KB_STAT,al
        sti
        clc
        ret
sc_busy:
        sti
sc_next:
        loop    sc_wait
        dec     bx
        jnz     sc_wait
        stc
        ret

;---------------------------------------------------------------------
; waitst: wait until status bits 5/4 match 'mode'.
; Out: CF set on timeout.
;---------------------------------------------------------------------
waitst: mov     ah,0                    ; expected bits 5/4
        cmp     mode,CMD_OFF
        jne     ws_low
        mov     ah,ST_OFF
ws_low: cmp     mode,CMD_LOW
        jne     ws_go
        mov     ah,ST_LOW
ws_go:  mov     bx,20
        xor     cx,cx
ws_loop:
        in      al,KB_STAT
        and     al,ST_MODE
        cmp     al,ah
        je      ws_ok
        loop    ws_loop
        dec     bx
        jnz     ws_loop
        stc
        ret
ws_ok:  clc
        ret

CSEG    ENDS
        END     START
