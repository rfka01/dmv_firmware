# asm48.py - kleiner Zwei-Pass-Assembler fuer MCS-48/UPI-41 (8741)
#
#   Syntax:  [marke:] BEFEHL operanden   ; Kommentar
#            ORG ausdruck  |  name EQU ausdruck  |  DB a,b,...
#   Zahlen:  12, 0Ch, 0x0C, 'A';  Ausdruecke mit + - * ( ) und LOW(x)
#   Bedingte Spruenge und DJNZ muessen in derselben 256-Byte-Seite bleiben,
#   JMP/CALL innerhalb von 2 KB. Ergebnis: dict adresse -> byte.
import re
from upi41 import T

REVU = {}
for o, (t, n, c) in T.items():
    k = '$j' if '$j' in t else ('$J' if '$J' in t else None)
    REVU[t.upper()] = (o, k)

def num(s, sym):
    s = s.strip()
    s = re.sub(r'LOW\(([^)]*)\)', r'((\1)&255)', s, flags=re.I)
    s = re.sub(r'HIGH\(([^)]*)\)', r'(((\1)>>8)&255)', s, flags=re.I)
    def rep(m):
        w = m.group(0)
        if re.fullmatch(r'[0-9][0-9A-Fa-f]*[hH]', w): return str(int(w[:-1], 16))
        if re.fullmatch(r'0[xX][0-9A-Fa-f]+', w): return str(int(w, 16))
        if re.fullmatch(r'[0-9]+', w): return w
        if w.upper() in sym: return str(sym[w.upper()])
        raise KeyError(w)
    s = re.sub(r"'(.)'", lambda m: str(ord(m.group(1))), s)
    s = re.sub(r'[A-Za-z_0-9.]+', rep, s)
    return eval(s, {}, {})

def assemble(text):
    lines = []
    for ln, raw in enumerate(text.splitlines(), 1):
        s = raw.split(';')[0].rstrip()
        if not s.strip(): continue
        lab = None
        m = re.match(r'^\s*([A-Za-z_][A-Za-z_0-9]*):(.*)$', s)
        if m: lab, s = m.group(1).upper(), m.group(2)
        lines.append((ln, raw, lab, s.strip()))
    sym = {}
    for pas in (1, 2):
        pc = 0; out = {}
        for ln, raw, lab, s in lines:
            m = re.match(r'^([A-Za-z_][A-Za-z_0-9]*)\s+EQU\s+(.*)$', s, re.I)
            if m:
                if pas == 1 or True:
                    try: sym[m.group(1).upper()] = num(m.group(2), sym)
                    except KeyError:
                        if pas == 2: raise
                continue
            if lab: sym[lab] = pc
            if not s: continue
            mn = s.split(None, 1); opc = mn[0].upper(); arg = mn[1] if len(mn) > 1 else ''
            if opc == 'ORG':
                pc = num(arg, sym); continue
            if opc == 'DB':
                for a in arg.split(','):
                    v = num(a, sym) if pas == 2 else 0
                    out[pc] = v & 0xFF; pc += 1
                continue
            ops = [a.strip() for a in arg.split(',')] if arg else []
            t = None; kind = None; val = None
            o2 = list(ops)
            for i, a in enumerate(o2):
                if a.startswith('#'): val = a[1:]; o2[i] = '#D'; kind = 'd'
            key = (opc + (' ' + ','.join(o2) if o2 else '')).upper()
            if key in REVU: t = key
            elif ops and kind is None:
                key = (opc + ' ' + ','.join(ops[:-1] + ['$J'])).upper()
                if key in REVU: t = key; val = ops[-1]
            if t is None:
                raise SyntaxError(f'Zeile {ln}: unbekannt: {raw.strip()}')
            o, kind2 = REVU[t]
            if kind2: kind = kind2
            n = T[o][1]
            if n == 1:
                out[pc] = o; pc += 1; continue
            v = 0
            if pas == 2:
                v = num(val, sym)
                if kind == '$j':
                    if (v & 0xF00) != ((pc + 1) & 0xF00):
                        raise ValueError(f'Zeile {ln}: Sprung aus der Seite {pc:03X}->{v:03X}')
                    v &= 0xFF
                elif kind == '$J':
                    if t.startswith('JMP ') or t.startswith('CALL '):
                        o = (o & 0x1F) | (((v >> 8) & 7) << 5)
                    v &= 0xFF
            out[pc] = o; out[pc + 1] = v & 0xFF; pc += 2
    return out, sym
