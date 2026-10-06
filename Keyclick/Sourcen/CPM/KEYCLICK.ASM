;=====================================================================
; KEYCLICK.COM (CP/M-80) - switch the key click of the NCR Decision
;                          Mate V on or off
;
;   KEYCLICK            show the current setting
;   KEYCLICK ON         key click on
;   KEYCLICK OFF        key click off
;   KEYCLICK LOW        just a faint tick
;
; Runs on the Z80 of the mainboard under CP/M-80 2.2 and needs the
; patched mainboard keyboard controller (8741, ROM 32678 with key
; click patch).
;
; Hardware:
;   The click comes from the speaker on the mainboard, driven by P2.0
;   of the 8741 keyboard controller. The patched firmware has three
;   additional commands:
;     02h  key click off           03h  key click on
;     04h  faint tick: one 0.5 ms pulse instead of a tone
;   and reports in the user bits of its status register:
;     bit 6  patch present (set after the self test, i.e. after boot)
;     bit 5  key click off         bit 4  tick (LOW)
;   40h data, 41h command/status, status bit 1 = IBF (input buffer
;   full). Every reset switches the click on again.
;
;   The original firmware must never get commands 02h..05h: it then
;   loops in its command dispatcher and stops scanning the keyboard
;   until the next command byte. KEYCLICK therefore checks bit 6 first
;   and does nothing without the patch. The first patch version knows
;   02h/03h but not 04h; if LOW is not confirmed, KEYCLICK sends 03h,
;   which ends the loop, and reports that LOW is missing.
;
; 8080 instructions only. Build on CP/M-80:
;   ASM KEYCLICK
;   LOAD KEYCLICK
;=====================================================================

BDOS    EQU     5
TAIL    EQU     80H             ; command tail: length, characters

KBSTAT  EQU     41H             ; 8741 status (read) / command (write)
STIBF   EQU     02H             ; input buffer full
STLOW   EQU     10H             ; tick (LOW)
STOFF   EQU     20H             ; key click off
STMODE  EQU     30H
STPAT   EQU     40H             ; key click patch present
CMDOFF  EQU     02H
CMDON   EQU     03H
CMDLOW  EQU     04H

        ORG     100H

START:  LXI     H,TAIL          ; terminate the tail with 0
        MOV     E,M
        MVI     D,0
        INX     H
        DAD     D
        MVI     M,0
        LXI     H,TAIL+1
        CALL    SKIPSP
        ORA     A
        JZ      CHECK           ; no parameter: show
        LXI     D,PARAM         ; copy up to 4 characters
        MVI     B,4
CPY:    CPI     ' '
        JZ      CPYEND
        CPI     9
        JZ      CPYEND
        ORA     A
        JZ      CPYEND
        MOV     C,A
        MOV     A,B
        ORA     A
        JZ      USAGE           ; word too long
        MOV     A,C
        CPI     'a'
        JC      CPYUP
        CPI     'z'+1
        JNC     CPYUP
        SUI     20H
CPYUP:  STAX    D
        INX     D
        DCR     B
        MOV     A,M
        INX     H
        JMP     CPY
CPYEND: DCX     H               ; nothing may follow
        CALL    SKIPSP
        ORA     A
        JNZ     USAGE
        LXI     H,KEYTAB        ; look the word up
LOOK:   MOV     A,M
        ORA     A
        JZ      USAGE
        PUSH    H
        LXI     D,PARAM
        MVI     B,4
LCMP:   LDAX    D
        CMP     M
        JNZ     LNEXT
        INX     H
        INX     D
        DCR     B
        JNZ     LCMP
        MOV     A,M
        STA     MODE
        POP     H
        JMP     CHECK
LNEXT:  POP     H
        LXI     D,5
        DAD     D
        JMP     LOOK

CHECK:  IN      KBSTAT
        ANI     STPAT
        JZ      NOPAT
        LDA     MODE
        ORA     A
        JZ      SHOW

        CALL    SENDCMD         ; CY = timeout
        JC      TIMEOUT
        CALL    WAITST          ; wait until the status shows it
        JNC     SHOW
        LDA     MODE
        CPI     CMDLOW
        JNZ     TIMEOUT
        MVI     A,CMDON         ; first patch version: 03h ends
        STA     MODE            ; its dispatcher loop
        CALL    SENDCMD
        JC      TIMEOUT
        CALL    WAITST
        JC      TIMEOUT
        LXI     D,MSGNOLO
        JMP     PRINT

SHOW:   IN      KBSTAT
        MOV     B,A
        LXI     D,MSGOFF
        ANI     STOFF
        JNZ     PRINT
        MOV     A,B
        LXI     D,MSGLOW
        ANI     STLOW
        JNZ     PRINT
        LXI     D,MSGON
        JMP     PRINT

USAGE:  LXI     D,MSGUSE
        JMP     PRINT
NOPAT:  LXI     D,MSGNOP
        JMP     PRINT
TIMEOUT:
        LXI     D,MSGTIME
PRINT:  MVI     C,9
        CALL    BDOS
        MVI     C,0
        JMP     BDOS

;---------------------------------------------------------------------
; SKIPSP: next character after blanks and tabs -> A, HL behind it
;---------------------------------------------------------------------
SKIPSP: MOV     A,M
        INX     H
        CPI     ' '
        JZ      SKIPSP
        CPI     9
        JZ      SKIPSP
        RET

;---------------------------------------------------------------------
; SENDCMD: write command MODE to the 8741 as soon as its input buffer
; is empty. The 8741 may be busy for seconds (music), so wait long.
; Out: CY set on timeout.
;---------------------------------------------------------------------
SENDCMD:
        MVI     D,20            ; about 20 x 65536 polls
        LXI     H,0
SC1:    IN      KBSTAT
        ANI     STIBF
        JNZ     SC2
        LDA     MODE
        OUT     KBSTAT
        ORA     A               ; CY = 0
        RET
SC2:    DCX     H
        MOV     A,H
        ORA     L
        JNZ     SC1
        DCR     D
        JNZ     SC1
        STC
        RET

;---------------------------------------------------------------------
; WAITST: wait until status bits 5/4 match MODE.
; Out: CY set on timeout.
;---------------------------------------------------------------------
WAITST: LDA     MODE
        MVI     E,0             ; expected bits 5/4
        CPI     CMDOFF
        JNZ     WS1
        MVI     E,STOFF
WS1:    CPI     CMDLOW
        JNZ     WS2
        MVI     E,STLOW
WS2:    MVI     D,20
        LXI     H,0
WS3:    IN      KBSTAT
        ANI     STMODE
        CMP     E
        RZ                      ; equal: CY = 0
        DCX     H
        MOV     A,H
        ORA     L
        JNZ     WS3
        DCR     D
        JNZ     WS3
        STC
        RET

;=====================================================================
;  Data
;=====================================================================

MODE:   DB      0               ; 0 show, CMDOFF, CMDON, CMDLOW
PARAM:  DB      0,0,0,0         ; parameter, upper case

KEYTAB: DB      'ON',0,0,CMDON
        DB      'OFF',0,CMDOFF
        DB      'LOW',0,CMDLOW
        DB      0

MSGON:  DB      'Tastaturklick ist AN',13,10,'$'
MSGOFF: DB      'Tastaturklick ist AUS',13,10,'$'
MSGLOW: DB      'Tastaturklick ist LOW',13,10,'$'
MSGNOLO:
        DB      'Diese Version des Klick-Patches kennt LOW noch nicht.',13,10
        DB      'Der Klick bleibt AN. Bitte das neue ROM einsetzen.',13,10,'$'
MSGNOP: DB      'Der Tastatur-Controller (8741 auf der Hauptplatine) hat die',13,10
        DB      'Originalfirmware ohne Klick-Schalter. KEYCLICK braucht das',13,10
        DB      'gepatchte ROM 32678 (DMV_MB_8741_32678_KEYCLICK.BIN).',13,10,'$'
MSGTIME:
        DB      'Der Tastatur-Controller antwortet nicht.',13,10,'$'
MSGUSE: DB      'KEYCLICK - Tastaturklick der NCR Decision Mate V',13,10
        DB      13,10
        DB      '  KEYCLICK        zeigt die Einstellung',13,10
        DB      '  KEYCLICK ON     Tastaturklick an',13,10
        DB      '  KEYCLICK OFF    Tastaturklick aus',13,10
        DB      '  KEYCLICK LOW    nur ein leiser Knack',13,10
        DB      13,10
        DB      'Nach jedem Reset ist der Klick wieder an.',13,10,'$'

        END     START
