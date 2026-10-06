# MK68K.PY - make a CP/M-68K program file from position-independent code:
#   python mk68k.py KEYC68.BIN KEYCLICK.68K
# Header (28 bytes, big endian): 601Ah, text size, data size, bss size,
# symbol size, reserved, text start, relocation flag (0 = relocation
# words follow). One relocation word per text word, all 0 (absolute):
# the code uses PC-relative addresses only.
import sys, struct
text = open(sys.argv[1], 'rb').read()
text += bytes(len(text) & 1)
hdr = struct.pack('>HIIIIIIH', 0x601A, len(text), 0, 0, 0, 0, 0, 0)
open(sys.argv[2], 'wb').write(hdr + text + bytes(len(text)))
print(f"{sys.argv[2]}: text {len(text)} bytes")
