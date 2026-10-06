# MKROM.PY - key click patch for the keyboard controller on the mainboard
#            of the NCR Decision Mate V (8741, ROM 32678)
#
#   python MKROM.PY dmv_mb_8741_32678.bin dmv_mb_8741_32678_keyclick.bin
#
# Input: the original ROM 32678 (1 KB, CRC32 50d1dc4c, as in MAME).
# Output: the patched ROM (CRC32 82709883).
#
# What the original firmware does:
#   The speaker on the mainboard hangs on P2.0 of the 8741. Whenever the
#   8741 has placed a key code in its output buffer (0CDh), it plays the
#   tone routine at 264h (R3=R4=6Fh, R5=0, R7=2: about 870 Hz, 20 ms).
#   There is no way to switch it off. The command dispatcher at 200h
#   (IN A,DBB / ADD A,#4 / JMPP @A) uses the table at 204h..20Bh:
#   0 self test, 1 country code, 2..5 unused (00h = jump to 200h, which
#   loops on the same byte), 6 music, 7 bell. The self test checks that
#   the sum of all 1024 bytes is 0 (error C0h otherwise).
#   The areas 10Ah..1FFh, 2BBh..2FFh and 366h..3FFh are never executed;
#   they hold leftovers of the EPROM programmer's buffer.
#
# The patch (all new code in unused areas):
#   RAM 29h   0 = normal click, 1 = off, 2 = LOW. The self test clears
#             the RAM, so every reset switches the click on again.
#   STS       user bits of the status register (port 41h on the host):
#             bit 6 = patch present, bit 5 = off, bit 4 = LOW
#   0CDh      JMP hook: flag 0 -> original click, 1 -> none, 2 -> LOW
#   cmd 02h   click off   cmd 03h  click on   cmd 04h  LOW
#   LOW       a single pulse on P2.0 (0.5 ms low) instead of a tone
#   240h      after a good self test: STS = 40h, then the original path
#   227h      end of command 1: restore STS (the original clears it)
#   3FFh      checksum byte
import sys, zlib

src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
assert len(d) == 1024 and zlib.crc32(d) == 0x50D1DC4C, "not ROM 32678"
assert sum(d) & 0xFF == 0

FLAG = 0x29                 # RAM address of the flag
LOW_R4 = 0x62               # pulse width: 98 x DJNZ (2 cycles of 2.5 us) = 0.49 ms

def assemble(org, prog):
    """Two-pass mini assembler. prog: labels (str) and tuples of bytes;
    ('lo', label) = low byte of the label, ('j', label) = target of a
    conditional jump or DJNZ (must be in the same 256-byte page)."""
    labels = {}; pc = org
    for it in prog:
        if isinstance(it, str): labels[it] = pc
        else: pc += len(it)
    out = []; pc = org
    for it in prog:
        if isinstance(it, str): continue
        for b in it:
            if isinstance(b, tuple):
                tgt = labels[b[1]]
                if b[0] == 'j': assert (tgt & 0xF00) == ((pc + 1) & 0xF00), b
                out.append(tgt & 0xFF)
            else:
                out.append(b)
            pc += 1
    return bytes(out), labels

# Page 2, free area from 2BBh
page2 = [
 'hook',  (0xB8, FLAG),                  # MOV  R0,#29h
          (0xF0,),                       # MOV  A,@R0
          (0xC6, ('j', 'norm')),         # JZ   norm
          (0x07,),                       # DEC  A
          (0xC6, ('j', 'skip')),         # JZ   skip      (1 = off)
          (0x24, 0x0A),                  # JMP  10Ah      (2 = LOW)
 'norm',  (0xBB, 0x6F, 0xBC, 0x6F,       # original click: MOV R3,#6Fh / MOV R4,#6Fh
           0xBD, 0x00, 0xBF, 0x02,       #   MOV R5,#0 / MOV R7,#2
           0x95, 0x44, 0x64),            #   CPL F0 / JMP 264h
 'skip',  (0x04, 0xA6),                  # JMP  0A6h      (main loop)
 'cmd2',  (0xB8, FLAG, 0xB0, 0x01, 0x44, ('lo', 'sts2')),   # flag = 1, set STS
 'cmd3',  (0xB8, FLAG, 0xB0, 0x00, 0x44, ('lo', 'sts2')),   # flag = 0, set STS
 'cmd4',  (0xB8, FLAG, 0xB0, 0x02, 0x44, ('lo', 'sts2')),   # flag = 2, set STS
 'init',  (0x23, 0x40, 0x90, 0x04, 0x32),                   # STS = 40h, JMP 032h
 'sts',   (0xB8, FLAG),                                     # MOV  R0,#29h
 'sts2',  (0xF0, 0x03, ('lo', 'tbl'),    # MOV A,@R0 / ADD A,#tbl
           0xA3, 0x90, 0x04, 0x60),      # MOVP A,@A / MOV STS,A / JMP 060h
 'tbl',   (0x40, 0x60, 0x50),            # STS for on / off / LOW
]
code2, L2 = assemble(0x2BB, page2)
assert 0x2BB + len(code2) <= 0x300

# Page 1, free area from 10Ah: LOW = one short pulse
page1 = [
 'low',   (0x9A, 0xFE),                  # ANL  P2,#FEh   speaker low
          (0xBC, LOW_R4),                # MOV  R4,#62h
 'd',     (0xEC, ('j', 'd')),            # DJNZ R4,$
          (0x8A, 0x01),                  # ORL  P2,#01h   speaker high again
          (0x04, 0xA6),                  # JMP  0A6h
]
code1, L1 = assemble(0x10A, page1)

d[0x2BB:0x2BB + len(code2)] = code2
d[0x10A:0x10A + len(code1)] = code1
d[0x0CD:0x0CF] = bytes([0x44, L2['hook'] & 0xFF])   # click -> hook
d[0x227:0x229] = bytes([0x44, L2['sts'] & 0xFF])    # end of cmd 1 -> restore STS
d[0x240:0x242] = bytes([0x44, L2['init'] & 0xFF])   # self test OK -> STS = 40h
d[0x206] = L2['cmd2'] & 0xFF                        # dispatcher table
d[0x207] = L2['cmd3'] & 0xFF
d[0x208] = L2['cmd4'] & 0xFF
d[0x3FF] = (d[0x3FF] - sum(d)) & 0xFF               # checksum
assert sum(d) & 0xFF == 0
open(dst, 'wb').write(d)
print(f"{dst}: CRC32 {zlib.crc32(d):08x}")
