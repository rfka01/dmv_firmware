# upi41.py - Disassembler und Simulator fuer den Intel 8741 (UPI-41, MCS-48-Kern)
# Zyklen: 1 Befehlszyklus = 15 Takte; bei 6 MHz 2,5 us.

# Befehlstabelle: opcode -> (Text, Laenge, Zyklen). Platzhalter: #d Datum,
# $j Sprung in der Seite, $J 11-Bit-Sprung/Call
T = {}
def op(o, t, n=1, c=1): T[o] = (t, n, c)
op(0x00,'NOP'); op(0x02,'OUT DBB,A'); op(0x03,'ADD A,#d',2,2)
for i in range(8):
    op(0x04 | i << 5, 'JMP $J', 2, 2); op(0x14 | i << 5, 'CALL $J', 2, 2)
    op(0x12 | i << 5, f'JB{i} $j', 2, 2)
op(0x05,'EN I'); op(0x07,'DEC A'); op(0x09,'IN A,P1',1,2); op(0x0A,'IN A,P2',1,2)
for i in range(4):
    op(0x0C+i, f'MOVD A,P{4+i}',1,2); op(0x3C+i, f'MOVD P{4+i},A',1,2)
    op(0x8C+i, f'ORLD P{4+i},A',1,2); op(0x9C+i, f'ANLD P{4+i},A',1,2)
for i in range(2):
    op(0x10+i, f'INC @R{i}'); op(0x20+i, f'XCH A,@R{i}'); op(0x30+i, f'XCHD A,@R{i}')
    op(0x40+i, f'ORL A,@R{i}'); op(0x50+i, f'ANL A,@R{i}'); op(0x60+i, f'ADD A,@R{i}')
    op(0x70+i, f'ADDC A,@R{i}'); op(0xA0+i, f'MOV @R{i},A'); op(0xB0+i, f'MOV @R{i},#d',2,2)
    op(0xD0+i, f'XRL A,@R{i}'); op(0xF0+i, f'MOV A,@R{i}')
op(0x13,'ADDC A,#d',2,2); op(0x15,'DIS I'); op(0x16,'JTF $j',2,2); op(0x17,'INC A')
for r in range(8):
    op(0x18+r, f'INC R{r}'); op(0x28+r, f'XCH A,R{r}'); op(0x48+r, f'ORL A,R{r}')
    op(0x58+r, f'ANL A,R{r}'); op(0x68+r, f'ADD A,R{r}'); op(0x78+r, f'ADDC A,R{r}')
    op(0xA8+r, f'MOV R{r},A'); op(0xB8+r, f'MOV R{r},#d',2,2); op(0xC8+r, f'DEC R{r}')
    op(0xD8+r, f'XRL A,R{r}'); op(0xE8+r, f'DJNZ R{r},$j',2,2); op(0xF8+r, f'MOV A,R{r}')
op(0x22,'IN A,DBB'); op(0x23,'MOV A,#d',2,2); op(0x25,'EN TCNTI'); op(0x26,'JNT0 $j',2,2)
op(0x27,'CLR A'); op(0x35,'DIS TCNTI'); op(0x36,'JT0 $j',2,2); op(0x37,'CPL A')
op(0x39,'OUTL P1,A',1,2); op(0x3A,'OUTL P2,A',1,2); op(0x42,'MOV A,T'); op(0x43,'ORL A,#d',2,2)
op(0x45,'STRT CNT'); op(0x46,'JNT1 $j',2,2); op(0x47,'SWAP A'); op(0x53,'ANL A,#d',2,2)
op(0x55,'STRT T'); op(0x56,'JT1 $j',2,2); op(0x57,'DA A'); op(0x62,'MOV T,A'); op(0x65,'STOP TCNT')
op(0x67,'RRC A'); op(0x76,'JF1 $j',2,2); op(0x77,'RR A'); op(0x83,'RET',1,2); op(0x85,'CLR F0')
op(0x86,'JOBF $j',2,2); op(0x89,'ORL P1,#d',2,2); op(0x8A,'ORL P2,#d',2,2); op(0x90,'MOV STS,A')
op(0x93,'RETR',1,2); op(0x95,'CPL F0'); op(0x96,'JNZ $j',2,2); op(0x97,'CLR C')
op(0x99,'ANL P1,#d',2,2); op(0x9A,'ANL P2,#d',2,2); op(0xA3,'MOVP A,@A',1,2); op(0xA5,'CLR F1')
op(0xA7,'CPL C'); op(0xB3,'JMPP @A',1,2); op(0xB5,'CPL F1'); op(0xB6,'JF0 $j',2,2)
op(0xC5,'SEL RB0'); op(0xC6,'JZ $j',2,2); op(0xC7,'MOV A,PSW'); op(0xD3,'XRL A,#d',2,2)
op(0xD5,'SEL RB1'); op(0xD6,'JNIBF $j',2,2); op(0xD7,'MOV PSW,A'); op(0xE3,'MOVP3 A,@A',1,2)
op(0xE5,'EN DMA'); op(0xE6,'JNC $j',2,2); op(0xE7,'RL A'); op(0xF5,'EN FLAGS')
op(0xF6,'JC $j',2,2); op(0xF7,'RLC A')

def dis(rom, pc):
    o = rom[pc & 0x3FF]
    if o not in T: return (f'DB {o:02X}h', 1)
    t, n, c = T[o]
    if n == 2:
        b = rom[(pc + 1) & 0x3FF]
        t = t.replace('#d', f'#{b:02X}h')
        t = t.replace('$j', f'{((pc + 1) & 0x700) | b:03X}h')
        t = t.replace('$J', f'{((o >> 5) << 8) | b:03X}h')
    return (t, n)


class UPI41:
    """8741 mit Host-Schnittstelle (DBB), Timer, P1/P2. Ein Aufruf von step()
    fuehrt einen Befehl aus und zaehlt die Zyklen in self.cyc."""
    def __init__(self, rom):
        self.rom = bytearray(rom)
        self.ram = bytearray(64)
        self.reset()

    def reset(self):
        self.pc = 0; self.a = 0; self.psw = 0x08; self.cyc = 0
        self.dbbin = 0; self.dbbout = 0; self.ibf = 0; self.obf = 0
        self.f1 = 0; self.sts = 0; self.t = 0; self.tf = 0
        self.trun = 0; self.tpre = 0; self.ie = 0; self.tie = 0
        self.inint = 0; self.p1 = 0xFF; self.p2 = 0xFF; self.flags = 0
        self.p1in = 0xFF; self.p2in = 0xFF; self.t0 = 1; self.t1 = 1
        self.p2log = []          # (zyklus, wert von P2.0) bei jeder Aenderung
        self.p2last = 1

    # --- Host-Seite ---
    def host_write(self, port, v):          # port 0 = Daten (40h), 1 = Befehl (41h)
        self.dbbin = v & 0xFF; self.ibf = 1; self.f1 = port & 1
    def host_status(self):
        return (self.sts & 0xF0) | (self.f1 << 3) | (self.f0() << 2) | (self.ibf << 1) | self.obf
    def host_read(self):
        self.obf = 0; return self.dbbout

    def p1_in(self):
        # Verdrahtung wie MAME dmv.cpp: P1.0 = NOT(SDATA AND NOT P1.1)
        sd = self.sdata() if hasattr(self, 'sdata') else 1
        b0 = 0 if (sd and not (self.p1 & 2)) else 1
        return 0xFE | b0

    # --- Hilfen ---
    def f0(self): return (self.psw >> 5) & 1
    def bs(self): return 0x18 if self.psw & 0x10 else 0
    def R(self, r): return self.ram[self.bs() + r]
    def setR(self, r, v): self.ram[self.bs() + r] = v & 0xFF
    def cy(self): return self.psw >> 7
    def setcy(self, c): self.psw = (self.psw & 0x7F) | (0x80 if c else 0)
    def fetch(self):
        b = self.rom[self.pc & 0x3FF]
        self.pc = (self.pc & 0x800) | ((self.pc + 1) & 0x7FF)
        return b
    def setp2(self, v):
        self.p2 = v & 0xFF
        b = self.p2 & 1
        if b != self.p2last:
            self.p2log.append((self.cyc, b)); self.p2last = b
    def push(self):
        sp = self.psw & 7
        self.ram[8 + 2 * sp] = self.pc & 0xFF
        self.ram[9 + 2 * sp] = ((self.pc >> 8) & 0x0F) | (self.psw & 0xF0)
        self.psw = (self.psw & 0xF8) | ((sp + 1) & 7)
    def pop(self, restore):
        sp = (self.psw - 1) & 7
        self.psw = (self.psw & 0xF8) | sp
        lo = self.ram[8 + 2 * sp]; hi = self.ram[9 + 2 * sp]
        self.pc = ((hi & 0x0F) << 8) | lo
        if restore: self.psw = (self.psw & 0x0F) | (hi & 0xF0)
    def add(self, v, c=0):
        r = self.a + v + c
        ac = ((self.a & 15) + (v & 15) + c) > 15
        self.psw = (self.psw & 0x3F) | (0x80 if r > 255 else 0) | (0x40 if ac else 0)
        self.a = r & 0xFF
    def jcond(self, cond):
        b = self.fetch()
        if cond: self.pc = ((self.pc - 1) & 0x700) | b

    def tick(self, n):
        self.cyc += n
        if self.trun:
            self.tpre += n
            while self.tpre >= 32:
                self.tpre -= 32
                self.t = (self.t + 1) & 0xFF
                if self.t == 0:
                    self.tf = 1
                    if self.tie: self.tirq = 1

    def step(self):
        # Interrupts: IBF (Vektor 3) vor Timer (Vektor 7)
        if not self.inint:
            if self.ie and self.ibf:
                self.inint = 1; self.push(); self.pc = 3; self.tick(2); return
            if self.tie and getattr(self, 'tirq', 0):
                self.tirq = 0; self.inint = 1; self.push(); self.pc = 7; self.tick(2); return
        pc0 = self.pc
        o = self.fetch()
        if o not in T: raise RuntimeError(f'unbekannter Befehl {o:02X} bei {pc0:03X}')
        cyc = T[o][2]
        a = self.a
        if o == 0x00: pass
        elif o == 0x02: self.dbbout = a; self.obf = 1
        elif o == 0x03: self.add(self.fetch())
        elif o & 0x1F == 0x04: b = self.fetch(); self.pc = ((o >> 5) << 8) | b
        elif o & 0x1F == 0x14: b = self.fetch(); self.push(); self.pc = ((o >> 5) << 8) | b
        elif o & 0x1F == 0x12: self.jcond((a >> (o >> 5)) & 1)
        elif o == 0x05: self.ie = 1
        elif o == 0x07: self.a = (a - 1) & 0xFF
        elif o == 0x09: self.a = self.p1 & self.p1_in()
        elif o == 0x0A: self.a = self.p2 & (self.p2_in() if hasattr(self, 'p2_in') else self.p2in)
        elif o in (0x10, 0x11): i = self.R(o & 1) & 0x3F; self.ram[i] = (self.ram[i] + 1) & 0xFF
        elif o == 0x13: self.add(self.fetch(), self.cy())
        elif o == 0x15: self.ie = 0
        elif o == 0x16: self.jcond(self.tf); self.tf = 0
        elif o == 0x17: self.a = (a + 1) & 0xFF
        elif 0x18 <= o <= 0x1F: self.setR(o & 7, self.R(o & 7) + 1)
        elif o in (0x20, 0x21): i = self.R(o & 1) & 0x3F; self.a, self.ram[i] = self.ram[i], a
        elif o == 0x22: self.a = self.dbbin; self.ibf = 0
        elif o == 0x23: self.a = self.fetch()
        elif o == 0x25: self.tie = 1
        elif o == 0x26: self.jcond(not self.t0)
        elif o == 0x27: self.a = 0
        elif 0x28 <= o <= 0x2F: r = o & 7; self.a = self.R(r); self.setR(r, a)
        elif o in (0x30, 0x31):
            i = self.R(o & 1) & 0x3F; m = self.ram[i]
            self.a = (a & 0xF0) | (m & 15); self.ram[i] = (m & 0xF0) | (a & 15)
        elif o == 0x35: self.tie = 0
        elif o == 0x36: self.jcond(self.t0)
        elif o == 0x37: self.a = a ^ 0xFF
        elif o == 0x39: self.p1 = a
        elif o == 0x3A: self.setp2(a)
        elif o in (0x40, 0x41): self.a = a | self.ram[self.R(o & 1) & 0x3F]
        elif o == 0x42: self.a = self.t
        elif o == 0x43: self.a = a | self.fetch()
        elif o == 0x45: self.trun = 1
        elif o == 0x46: self.jcond(not self.t1)
        elif o == 0x47: self.a = ((a << 4) | (a >> 4)) & 0xFF
        elif 0x48 <= o <= 0x4F: self.a = a | self.R(o & 7)
        elif o in (0x50, 0x51): self.a = a & self.ram[self.R(o & 1) & 0x3F]
        elif o == 0x53: self.a = a & self.fetch()
        elif o == 0x55: self.trun = 1; self.tpre = 0
        elif o == 0x56: self.jcond(self.t1)
        elif o == 0x57:
            if (a & 15) > 9 or self.psw & 0x40: a += 6
            if (a >> 4) > 9 or self.cy() or a > 255: a += 0x60; self.setcy(1)
            self.a = a & 0xFF
        elif 0x58 <= o <= 0x5F: self.a = a & self.R(o & 7)
        elif o in (0x60, 0x61): self.add(self.ram[self.R(o & 1) & 0x3F])
        elif o == 0x62: self.t = a
        elif o == 0x65: self.trun = 0
        elif o == 0x67: c = a & 1; self.a = (a >> 1) | (self.cy() << 7); self.setcy(c)
        elif 0x68 <= o <= 0x6F: self.add(self.R(o & 7))
        elif o in (0x70, 0x71): self.add(self.ram[self.R(o & 1) & 0x3F], self.cy())
        elif o == 0x76: self.jcond(self.f1)
        elif o == 0x77: self.a = ((a >> 1) | (a << 7)) & 0xFF
        elif 0x78 <= o <= 0x7F: self.add(self.R(o & 7), self.cy())
        elif o == 0x83: self.pop(False)
        elif o == 0x85: self.psw &= ~0x20
        elif o == 0x86: self.jcond(self.obf)
        elif o == 0x89: self.p1 |= self.fetch()
        elif o == 0x8A: self.setp2(self.p2 | self.fetch())
        elif o == 0x90: self.sts = a & 0xF0
        elif o == 0x93: self.pop(True); self.inint = 0
        elif o == 0x95: self.psw ^= 0x20
        elif o == 0x96: self.jcond(a != 0)
        elif o == 0x97: self.setcy(0)
        elif o == 0x99: self.p1 &= self.fetch()
        elif o == 0x9A: self.setp2(self.p2 & self.fetch())
        elif o in (0xA0, 0xA1): self.ram[self.R(o & 1) & 0x3F] = a
        elif o == 0xA3: self.a = self.rom[((pc0) & 0x300) | a]
        elif o == 0xA5: self.f1 = 0
        elif o == 0xA7: self.setcy(not self.cy())
        elif 0xA8 <= o <= 0xAF: self.setR(o & 7, a)
        elif o in (0xB0, 0xB1): self.ram[self.R(o & 1) & 0x3F] = self.fetch()
        elif o == 0xB3: self.pc = (pc0 & 0x300) | self.rom[(pc0 & 0x300) | a]
        elif o == 0xB5: self.f1 ^= 1
        elif o == 0xB6: self.jcond(self.f0())
        elif 0xB8 <= o <= 0xBF: self.setR(o & 7, self.fetch())
        elif o == 0xC5: self.psw &= ~0x10
        elif o == 0xC6: self.jcond(a == 0)
        elif o == 0xC7: self.a = self.psw | 0x08
        elif 0xC8 <= o <= 0xCF: self.setR(o & 7, self.R(o & 7) - 1)
        elif o in (0xD0, 0xD1): self.a = a ^ self.ram[self.R(o & 1) & 0x3F]
        elif o == 0xD3: self.a = a ^ self.fetch()
        elif o == 0xD5: self.psw |= 0x10
        elif o == 0xD6: self.jcond(not self.ibf)
        elif o == 0xD7: self.psw = a | 0x08
        elif 0xD8 <= o <= 0xDF: self.a = a ^ self.R(o & 7)
        elif o == 0xE3: self.a = self.rom[0x300 | a]
        elif o == 0xE5: pass
        elif o == 0xE6: self.jcond(not self.cy())
        elif o == 0xE7: self.a = ((a << 1) | (a >> 7)) & 0xFF
        elif 0xE8 <= o <= 0xEF:
            r = o & 7; self.setR(r, self.R(r) - 1); self.jcond(self.R(r) != 0)
        elif o in (0xF0, 0xF1): self.a = self.ram[self.R(o & 1) & 0x3F]
        elif o == 0xF5: self.flags = 1
        elif o == 0xF6: self.jcond(self.cy())
        elif o == 0xF7: c = a >> 7; self.a = ((a << 1) | self.cy()) & 0xFF; self.setcy(c)
        elif 0xF8 <= o <= 0xFF: self.a = self.R(o & 7)
        else: raise RuntimeError(f'nicht umgesetzt {o:02X}')
        self.tick(cyc)
