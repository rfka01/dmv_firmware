{ DTKSYN.INC - Ausgabe ueber die Synthesizer-Firmware des 8741

  Firmware: Keyclick v4 + Synthesizer (Befehl 05h). Erkennung am Status
  (Port 41h, Bits 7..4): C0h/E0h/D0h = vorhanden, F0h = laeuft gerade.
  Register (Paare Adresse, Wert an Port 40h):
    Stimme 1 Schrittweite 02h/03h, Phase hi 01h
    Stimme 2 Schrittweite 06h/07h, Phase hi 05h
    Stimme 3 Schrittweite 1Ch/1Dh, Phase hi 1Bh
    Rauschen 1Fh (0 aus, sonst Startwert)
  Frequenz = Schrittweite * 6726,6 Hz / 65536 (Schleifentakt mit
  Tastaturabfrage, gemessen in der 8741-Simulation).

  Takt: Solange der Synthesizer laeuft, ist Status-Bit 2 (F0) gesetzt,
  und die Bits 7..4 zeigen die oberen vier Bits des 8741-Timers: eine
  Uhr mit 1,28 ms je Schritt, die alle 20,48 ms umlaeuft. DMVTRAK muss
  sie also nur oefter als alle 20 ms ablesen.

  Stimmenzuteilung je Tick: Die klingenden Kanaele mit Instrumenten ohne
  Rauschen werden nach Prioritaet (dann juengster Anschlag) sortiert, die
  ersten drei bekommen eine Stimme. Ein Kanal behaelt seine Stimme,
  solange er dabei ist. Instrumente mit Rauschen (Schlagzeug) schalten
  beim Anschlag das Rauschen fuer hoechstens 4 Ticks ein. }

const
  KFreq    = 111973.0;            { MOD-Periode 856 (C-1) = 130,8 Hz }
  SynFS    = 6726.6;              { Schleifentakt der Firmware }
  VLoA: array[0..2] of byte = ($02, $06, $1C);
  VPhA: array[0..2] of byte = ($01, $05, $1B);

var
  SynAvail, SynOn, PitOk : boolean;
  PitPrev                : integer;
  Acc4                   : integer;   { Zeit in 80 us }
  VCh, VTrig             : array[0..2] of integer;   { Kanal je Stimme }
  VLo, VHi               : array[0..2] of integer;   { gesendet }
  CacPer, CacOfs, CacIns : array[0..7] of integer;
  CacLo, CacHi           : array[0..7] of integer;
  NoiseLeft              : integer;
  KH, KL                 : array[-48..48] of integer;   { 32-Bit-Konstanten }
  NoiseOn                : boolean;
  LastTrig               : array[0..7] of integer;

function ClkRead: integer;       { Uhr im Status: 0..15 }
begin
  ClkRead := Port[$41] shr 4;
end;

function KStat: integer;
begin
  KStat := Port[$41];
end;

function KWait: boolean;         { Eingangspuffer des 8741 frei? }
var K: integer;
begin
  K := 0;
  while ((Port[$41] and 2) <> 0) and (K < 20000) do K := K + 1;
  KWait := (Port[$41] and 2) = 0;
end;

procedure SynByte(B: integer);
begin
  if KWait then Port[$40] := B;
end;

procedure SynReg(A, V: integer);
begin
  SynByte(A);
  SynByte(V);
end;

function SynRunning: boolean;
begin
  SynRunning := (KStat and 4) <> 0;
end;

procedure SynInit;
var I, K: integer;
    R: real;
begin
  SynOn := false;
  SynMode := false;
  { F0 gesetzt: entweder laeuft noch ein Synthesizer von frueher oder
    gerade ein Tastenklick. Kurz warten; bleibt F0, Synthesizer beenden. }
  K := 0;
  while ((KStat and 4) <> 0) and (K < 20000) do K := K + 1;
  if (KStat and 4) <> 0 then
    if KWait then
    begin
      Port[$41] := $05;
      K := 0;
      while ((KStat and 4) <> 0) and (K < 20000) do K := K + 1;
    end;
  I := KStat and $F0;
  SynAvail := (I = $C0) or (I = $E0) or (I = $D0);
  PitOk := true;
  R := KFreq * 65536.0 / SynFS / 16.0;          { T = -48 }
  for I := -48 to 48 do
  begin
    KH[I] := Trunc(R / 65536.0);
    KL[I] := Real16(R - KH[I] * 65536.0);
    R := R * 1.0594630943593;
  end;
  UseSyn := SynAvail;
end;

function SynStart;               { forward in DTKPLAY }
var K, I: integer;
begin
  SynStart := false;
  if not (SynAvail and PitOk) then exit;
  if not SynRunning then
  begin
    if not KWait then exit;
    Port[$41] := $05;
    K := 0;                       { der 8741 kann noch eine Taste oder einen
                                    Ton bearbeiten (bis 0,6 s) }
    while (not SynRunning) and (K < 30000) do
    begin
      Pump;
      K := K + 1;
    end;
    if not SynRunning then exit;
  end;
  for I := 0 to 2 do
  begin
    VCh[I] := -1; VTrig[I] := 0;
    VLo[I] := -1; VHi[I] := -1;          { erzwingt das erste Schreiben }
  end;
  for I := 0 to 7 do begin CacPer[I] := -1; LastTrig[I] := 0; end;
  NoiseLeft := 0; NoiseOn := true;       { wird gleich ausgeschaltet }
  PitPrev := ClkRead; Acc4 := 0;
  SynOn := true;
  SynStart := true;
end;

procedure SynStop;
var K: integer;
begin
  if not SynOn then exit;
  SynOn := false;
  if SynRunning then
    if KWait then Port[$41] := $05;
  K := 0;
  while SynRunning and (K < 30000) do K := K + 1;
end;

{ Schrittweite fuer Kanal C, mit Arpeggio-Versatz und Transponierung:
  K(T) / Periode mit K(T) = 111973 * 65536 / 6964 * 2^(T/12), eine DIV }
procedure ChInc(C: integer; var Lo, Hi: integer);
var Ofs, P, T, Q: integer;
begin
  Ofs := 0;
  if (CEff[C] = 0) and (CPar[C] > 0) then
    case PlTick mod 3 of
      1: Ofs := CPar[C] shr 4;
      2: Ofs := CPar[C] and 15;
    end;
  P := CPer[C];
  if (P = CacPer[C]) and (Ofs = CacOfs[C]) and (CIns[C] = CacIns[C]) then
  begin
    Lo := CacLo[C]; Hi := CacHi[C];
    exit;
  end;
  CacPer[C] := P; CacOfs[C] := Ofs; CacIns[C] := CIns[C];
  T := Ofs + GTrans;
  if (CIns[C] >= 1) and (CIns[C] <= NIns) then T := T + ITrans[CIns[C]];
  if T < -48 then T := -48;
  if T > 48 then T := 48;
  if (P < 1) or (KH[T] >= P) then Q := -1          { zu hoch: 65535 }
  else UDiv(KH[T], KL[T], P, Q);
  Hi := (Q shr 8) and 255;
  Lo := Q and 255;
  if (Hi = 0) and (Lo = 0) then Lo := 1;
  CacLo[C] := Lo; CacHi[C] := Hi;
end;

procedure SynTick;
var Cand, Sel3: array[0..7] of integer;
    NC, NT, I, J, C, B, V, Lo, Hi, D: integer;
    InTop: boolean;
begin
  if not SynOn then exit;
  { Kandidaten: klingende Kanaele ohne Rauschen; Rauschen beim Anschlag }
  NC := 0;
  for C := 0 to NChan - 1 do
  begin
    if Sounding(C) then
    begin
      if INoise[CIns[C]] then
      begin
        if CTrig[C] <> LastTrig[C] then
        begin
          D := CRem[C];
          if (D < 0) or (D > 4) then D := 4;
          if D > NoiseLeft then NoiseLeft := D;
        end;
      end
      else
      begin
        Cand[NC] := C; NC := NC + 1;
      end;
    end;
    if C = 3 then PitTrack;
  end;
  PitTrack;
  { die besten drei auswaehlen }
  NT := 0;
  while (NT < 3) and (NC > 0) do
  begin
    B := 0;
    for I := 1 to NC - 1 do
    begin
      C := Cand[I]; J := Cand[B];
      if (IPrio[CIns[C]] > IPrio[CIns[J]]) or
         ((IPrio[CIns[C]] = IPrio[CIns[J]]) and (CTrig[C] > CTrig[J])) then B := I;
    end;
    Sel3[NT] := Cand[B]; NT := NT + 1;
    Cand[B] := Cand[NC - 1]; NC := NC - 1;
  end;
  PitTrack;
  { Stimmen freigeben, deren Kanal nicht mehr dabei ist }
  for V := 0 to 2 do
    if VCh[V] >= 0 then
    begin
      InTop := false;
      for I := 0 to NT - 1 do if Sel3[I] = VCh[V] then InTop := true;
      if not InTop then VCh[V] := -1;
    end;
  { neue Kanaele auf freie Stimmen }
  for I := 0 to NT - 1 do
  begin
    InTop := false;
    for V := 0 to 2 do if VCh[V] = Sel3[I] then InTop := true;
    if not InTop then
    begin
      V := 0;
      while VCh[V] >= 0 do V := V + 1;
      VCh[V] := Sel3[I];
      VTrig[V] := CTrig[Sel3[I]];
    end;
  end;
  { Register schreiben, nur Aenderungen }
  for V := 0 to 2 do
  begin
    C := VCh[V];
    if C < 0 then begin Lo := 0; Hi := 0; end
    else
    begin
      ChInc(C, Lo, Hi);
      if CTrig[C] <> VTrig[V] then
      begin
        VTrig[V] := CTrig[C];
        { gleicher Ton neu angeschlagen: einen Tick Pause }
        if (Lo = VLo[V]) and (Hi = VHi[V]) then
        begin
          Lo := 0; Hi := 0;
        end;
      end;
    end;
    PitTrack;
    if Lo <> VLo[V] then begin SynReg(VLoA[V], Lo); VLo[V] := Lo; end;
    if Hi <> VHi[V] then begin SynReg(VLoA[V] + 1, Hi); VHi[V] := Hi; end;
  end;
  for C := 0 to NChan - 1 do LastTrig[C] := CTrig[C];
  { Rauschen }
  if NoiseLeft > 0 then
  begin
    if not NoiseOn then begin SynReg($1F, $A5); NoiseOn := true; end;
    NoiseLeft := NoiseLeft - 1;
  end
  else if NoiseOn then
  begin
    SynReg($1F, 0);
    NoiseOn := false;
  end;
end;

procedure PitTrack;              { Uhr ablesen, oefter als alle 20 ms }
var V: integer;
begin
  if not SynOn then exit;
  V := ClkRead;
  Acc4 := Acc4 + ((V - PitPrev) and 15) * 16;   { in 80 us }
  PitPrev := V;
  if Acc4 > 30000 then Acc4 := 30000;
end;

procedure SynService;            { Zeit messen, faellige Ticks rechnen }
var N: integer;
begin
  if not SynOn then exit;
  PitTrack;
  N := 0;
  while (Acc4 >= TickLen80) and (N < 8) and not PlEnd do
  begin
    Acc4 := Acc4 - TickLen80;
    if not SynRunning then       { z. B. Glocke vom BIOS: neu starten }
    begin
      SynOn := false;
      if not SynStart then begin PlEnd := true; exit; end;
    end;
    GenTick;
    N := N + 1;
  end;
  if N >= 8 then Acc4 := 0;       { zu weit zurueck: nicht nachholen }
end;
