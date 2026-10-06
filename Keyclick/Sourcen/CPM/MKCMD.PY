# MKCMD.PY - make a CP/M-86 CMD file (8080 memory model) from a flat
# binary assembled for offset 100h:
#   python mkcmd.py KEYC86.BIN KEYCLICK.CMD
# Header: one code group (type 1); length, minimum and maximum in
# paragraphs; base 0 = relocatable. The group image starts with the
# 256-byte base page, which the loader fills in.
import sys, struct
code = open(sys.argv[1], 'rb').read()
image = bytes(256) + code
image += bytes(-len(image) % 16)
paras = len(image) // 16
hdr = bytearray(128)
struct.pack_into('<BHHHH', hdr, 0, 1, paras, 0, paras, paras)
open(sys.argv[2], 'wb').write(bytes(hdr) + image)
print(f"{sys.argv[2]}: {paras} paragraphs")
