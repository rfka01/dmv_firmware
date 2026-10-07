# MKSYNTH.PY - Keyclick v4 + Synthesizer fuer den Tastatur-Controller der DMV
#
#   python MKSYNTH.PY dmv_mb_8741_32678.bin dmv_mb_8741_32678_synth.bin
#
# Eingabe: Original-ROM 32678 (CRC32 50d1dc4c). Erzeugt zuerst genau das
# Keyclick-ROM v4 (CRC32 82709883, wie MKROM.PY) und setzt darauf:
#   114h..1FFh  Synthesizer und Division des Rennwerks (Seite 1)
#   2BBh..2FFh  Keyclick in gleicher Funktion, Befehle 02h..04h zusammen-
#               gefasst; Status C0h/E0h/D0h (Bit 7 = Synthesizer); Befehl
#               05h; Ergebnisausgabe des Rennwerks
#   356h..3FEh  Tastaturabfrage im Synthesizer, Rennwerk (ab Eintrag 43
#               der Tontabelle; die Eintraege 43..50 sind unbrauchbar)
#   206h..209h  Verteiler: 02h..04h -> cmd234, 05h -> cmd5
#   0CDh/227h/240h  Spruenge in den Keyclick-Teil
#   3FFh      Pruefsumme
import sys, os, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from asm48 import assemble

src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, 'rb').read())
assert len(d) == 1024 and zlib.crc32(d) == 0x50D1DC4C, "nicht ROM 32678"

# ---- Keyclick v4 (wie MKROM.PY) ----
kc = '''
FLAG EQU 29h
        ORG 10Ah
low:    ANL P2,#0FEh
        MOV R4,#62h
d:      DJNZ R4,d
        ORL P2,#01h
        JMP 0A6h
        ORG 2BBh
hook:   MOV R0,#FLAG
        MOV A,@R0
        JZ norm
        DEC A
        JZ skip
        JMP 10Ah
norm:   MOV R3,#6Fh
        MOV R4,#6Fh
        MOV R5,#00h
        MOV R7,#02h
        CPL F0
        JMP 264h
skip:   JMP 0A6h
cmd2:   MOV R0,#FLAG
        MOV @R0,#01h
        JMP sts2
cmd3:   MOV R0,#FLAG
        MOV @R0,#00h
        JMP sts2
cmd4:   MOV R0,#FLAG
        MOV @R0,#02h
        JMP sts2
init:   MOV A,#40h
        MOV STS,A
        JMP 032h
sts:    MOV R0,#FLAG
sts2:   MOV A,@R0
        ADD A,#LOW(tbl)
        MOVP A,@A
        MOV STS,A
        JMP 060h
tbl:    DB 40h, 60h, 50h
'''
out, L = assemble(kc)
for a, v in out.items(): d[a] = v
d[0x0CD:0x0CF] = bytes([0x44, L['HOOK'] & 0xFF])
d[0x227:0x229] = bytes([0x44, L['STS'] & 0xFF])
d[0x240:0x242] = bytes([0x44, L['INIT'] & 0xFF])
d[0x206] = L['CMD2'] & 0xFF; d[0x207] = L['CMD3'] & 0xFF; d[0x208] = L['CMD4'] & 0xFF
d[0x3FF] = (d[0x3FF] - sum(d)) & 0xFF
assert zlib.crc32(d) == 0x82709883, "Keyclick v4 weicht ab"

# ---- Synthesizer, Rennwerk, Keyclick kompakt (Seite 2 neu) ----
here = os.path.dirname(os.path.abspath(__file__))
out, S = assemble(open(os.path.join(here, 'SYNTH.A48')).read())
regions = [(0x114, 0x1FF), (0x2BB, 0x2FF), (0x356, 0x3FE)]
for a in out:
    assert any(lo <= a <= hi for lo, hi in regions), f"{a:03X} liegt ausserhalb der freien Bereiche"
for a in range(0x2BB, 0x300): d[a] = 0          # alte Seite-2-Ergaenzung ersetzen
for a, v in out.items(): d[a] = v
d[0x0CD:0x0CF] = bytes([0x44, S['HOOK'] & 0xFF])  # Klick -> hook
d[0x227:0x229] = bytes([0x44, S['STS'] & 0xFF])   # Ende Befehl 01h -> Status
d[0x240:0x242] = bytes([0x44, S['INIT'] & 0xFF])  # Selbsttest gut -> Status C0h
for a in (0x206, 0x207, 0x208): d[a] = S['CMD234'] & 0xFF
d[0x209] = S['CMD5'] & 0xFF
used = {pg: [a for a in out if a >> 8 == pg] for pg in (1, 2, 3)}
d[0x3FF] = 0
d[0x3FF] = (-sum(d)) & 0xFF
assert sum(d) & 0xFF == 0
open(dst, 'wb').write(d)
for pg, (lo, hi) in zip((1, 2, 3), regions):
    print(f"Seite {pg}: {min(used[pg]):03X}h..{max(used[pg]):03X}h, frei {hi - max(used[pg])} Byte")
print(f"{dst}: CRC32 {zlib.crc32(d):08x}")
