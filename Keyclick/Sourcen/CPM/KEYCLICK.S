| KEYCLICK.68K (CP/M-68K) - switch the key click of the NCR Decision
|                           Mate V on or off
| (GNU as, m68k:  m68k-linux-gnu-as -m68000 --register-prefix-optional)
|
|   KEYCLICK            show the current setting
|   KEYCLICK ON         key click on
|   KEYCLICK OFF        key click off
|   KEYCLICK LOW        just a faint tick
|
| Runs on the 68008 of the K234 under CP/M-68K and needs the patched
| mainboard keyboard controller (8741, ROM 32678 with key click patch).
|
| Hardware:
|   The K234 sees the DMV I/O ports at FFF00h + port, so the 8741 is at
|   FFF40h (data) and FFF41h (command/status), status bit 1 = IBF.
|   The click comes from the speaker on the mainboard, driven by P2.0
|   of the 8741. The patched firmware has three additional commands:
|     02h  key click off           03h  key click on
|     04h  faint tick: one 0.5 ms pulse instead of a tone
|   and reports in the user bits of its status register:
|     bit 6  patch present (set after the self test, i.e. after boot)
|     bit 5  key click off         bit 4  tick (LOW)
|   Every reset switches the click on again.
|
|   The original firmware must never get commands 02h..05h: it then
|   loops in its command dispatcher and stops scanning the keyboard
|   until the next command byte. KEYCLICK therefore checks bit 6 first
|   and does nothing without the patch. The first patch version knows
|   02h/03h but not 04h; if LOW is not confirmed, KEYCLICK sends 03h,
|   which ends the loop, and reports that LOW is missing.
|
|   Unlike the 8088 versions this one does not lock out interrupts
|   around the final IBF test and the write (that would need the
|   supervisor state); the BIOS only writes to the 8741 while it
|   outputs to the console, which never overlaps with KEYCLICK.
|
| The code is position independent (PC-relative only), so the program
| file needs no relocation (all relocation words are 0). MK68K.PY
| builds the CP/M-68K program file (header 601Ah) from the binary.

        .equ    KBSTAT, 0xFFF41         | 8741 status (read) / command (write)
        .equ    ST_IBF, 0x02            | input buffer full
        .equ    ST_LOW, 0x10            | tick (LOW)
        .equ    ST_OFF, 0x20            | key click off
        .equ    ST_MODE, 0x30
        .equ    ST_PAT, 0x40            | key click patch present
        .equ    CMD_OFF, 2
        .equ    CMD_ON, 3
        .equ    CMD_LOW, 4

        .text
start:  move.l  4(sp),a0                | base page
        lea     0x80(a0),a0             | command tail: length, characters
        moveq   #0,d0
        move.b  (a0)+,d0
        clr.b   0(a0,d0.w)              | end the tail with 0
        bsr     skipsp
        tst.b   d0
        beq     check                   | no parameter: show
        lea     param(pc),a1            | copy up to 4 characters
        moveq   #4,d2
cpy:    cmp.b   #' ',d0
        beq.s   cpyend
        cmp.b   #9,d0
        beq.s   cpyend
        tst.b   d0
        beq.s   cpyend
        tst.w   d2
        beq     usage                   | word too long
        cmp.b   #'a',d0
        blo.s   cpyup
        cmp.b   #'z',d0
        bhi.s   cpyup
        sub.b   #0x20,d0
cpyup:  move.b  d0,(a1)+
        subq.w  #1,d2
        move.b  (a0)+,d0
        bra.s   cpy
cpyend: subq.l  #1,a0                   | nothing may follow
        bsr     skipsp
        tst.b   d0
        bne     usage
        lea     keytab(pc),a1           | look the word up
        lea     param(pc),a2
        move.l  (a2),d1
look:   tst.b   (a1)
        beq     usage
        cmp.l   (a1),d1
        beq.s   found
        addq.l  #6,a1
        bra.s   look
found:  move.b  4(a1),d0
        lea     mode(pc),a2
        move.b  d0,(a2)

check:  move.b  KBSTAT,d0
        and.b   #ST_PAT,d0
        beq     nopat
        move.b  mode(pc),d0
        beq.s   show

        bsr     sendcmd                 | CC = ok, CS = timeout
        bcs     timeout
        bsr     waitst                  | wait until the status shows it
        bcc.s   show
        move.b  mode(pc),d0
        cmp.b   #CMD_LOW,d0
        bne     timeout
        lea     mode(pc),a2             | first patch version: 03h ends
        move.b  #CMD_ON,(a2)            | its dispatcher loop
        bsr     sendcmd
        bcs     timeout
        bsr     waitst
        bcs     timeout
        lea     msgnolo(pc),a0
        bra.s   print

show:   move.b  KBSTAT,d0
        lea     msgoff(pc),a0
        btst    #5,d0
        bne.s   print
        lea     msglow(pc),a0
        btst    #4,d0
        bne.s   print
        lea     msgon(pc),a0
        bra.s   print

usage:  lea     msguse(pc),a0
        bra.s   print
nopat:  lea     msgnop(pc),a0
        bra.s   print
timeout:
        lea     msgtime(pc),a0
print:  move.l  a0,d1
        moveq   #9,d0                   | print string
        trap    #2
        moveq   #0,d0                   | system reset
        trap    #2

|---------------------------------------------------------------------
| skipsp: next character after blanks and tabs -> d0, a0 behind it
|---------------------------------------------------------------------
skipsp: move.b  (a0)+,d0
        cmp.b   #' ',d0
        beq.s   skipsp
        cmp.b   #9,d0
        beq.s   skipsp
        rts

|---------------------------------------------------------------------
| sendcmd: write command 'mode' to the 8741 as soon as its input
| buffer is empty. The 8741 may be busy for seconds (music).
| Out: carry set on timeout.
|---------------------------------------------------------------------
sendcmd:
        moveq   #19,d3                  | about 20 x 65536 polls
sc_out: moveq   #-1,d2
sc_in:  move.b  KBSTAT,d0
        and.b   #ST_IBF,d0
        bne.s   sc_next
        move.b  mode(pc),KBSTAT
        and.b   #0xFE,ccr               | carry clear
        rts
sc_next:
        dbra    d2,sc_in
        dbra    d3,sc_out
        or.b    #0x01,ccr               | carry set
        rts

|---------------------------------------------------------------------
| waitst: wait until status bits 5/4 match 'mode'.
| Out: carry set on timeout.
|---------------------------------------------------------------------
waitst: moveq   #0,d1                   | expected bits 5/4
        move.b  mode(pc),d0
        cmp.b   #CMD_OFF,d0
        bne.s   ws1
        moveq   #ST_OFF,d1
ws1:    cmp.b   #CMD_LOW,d0
        bne.s   ws2
        moveq   #ST_LOW,d1
ws2:    moveq   #19,d3
ws_out: moveq   #-1,d2
ws_in:  move.b  KBSTAT,d0
        and.b   #ST_MODE,d0
        cmp.b   d1,d0
        beq.s   ws_ok
        dbra    d2,ws_in
        dbra    d3,ws_out
        or.b    #0x01,ccr
        rts
ws_ok:  and.b   #0xFE,ccr
        rts

|=====================================================================
|  Data (in the text segment, written through PC-relative addresses)
|=====================================================================

mode:   .byte   0                       | 0 show, CMD_OFF, CMD_ON, CMD_LOW
        .even
param:  .byte   0,0,0,0                 | parameter, upper case

keytab: .ascii  "ON"
        .byte   0,0,CMD_ON,0
        .ascii  "OFF"
        .byte   0,CMD_OFF,0
        .ascii  "LOW"
        .byte   0,CMD_LOW,0
        .byte   0

msgon:  .ascii  "Tastaturklick ist AN\r\n$"
msgoff: .ascii  "Tastaturklick ist AUS\r\n$"
msglow: .ascii  "Tastaturklick ist LOW\r\n$"
msgnolo:
        .ascii  "Diese Version des Klick-Patches kennt LOW noch nicht.\r\n"
        .ascii  "Der Klick bleibt AN. Bitte das neue ROM einsetzen.\r\n$"
msgnop: .ascii  "Der Tastatur-Controller (8741 auf der Hauptplatine) hat die\r\n"
        .ascii  "Originalfirmware ohne Klick-Schalter. KEYCLICK braucht das\r\n"
        .ascii  "gepatchte ROM 32678 (DMV_MB_8741_32678_KEYCLICK.BIN).\r\n$"
msgtime:
        .ascii  "Der Tastatur-Controller antwortet nicht.\r\n$"
msguse: .ascii  "KEYCLICK - Tastaturklick der NCR Decision Mate V\r\n"
        .ascii  "\r\n"
        .ascii  "  KEYCLICK        zeigt die Einstellung\r\n"
        .ascii  "  KEYCLICK ON     Tastaturklick an\r\n"
        .ascii  "  KEYCLICK OFF    Tastaturklick aus\r\n"
        .ascii  "  KEYCLICK LOW    nur ein leiser Knack\r\n"
        .ascii  "\r\n"
        .ascii  "Nach jedem Reset ist der Klick wieder an.\r\n$"
        .even
