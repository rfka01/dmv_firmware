program SynDemo;
{ SYNDEMO - Vorfuehrung des Synthesizers im Tastatur-Controller der DMV:
  Tonleiter, Akkord, Arpeggio, Klangeffekte und ein Kanon mit Schlagzeug.
  Braucht DMVSYN.INC. Den Takt liefert die Uhr des 8741 (1,28 ms je
  Schritt), nicht der 8088. ESC beendet das Demo nach dem laufenden Teil
  (im Kanon sofort).
  Code: Claude (Anthropic), Idee und Prompts: rfka01 - Lizenz MIT }
{$C-}
type RegPack = record AX, BX, CX, DX, BP, SI, DI, DS, ES, Flags: integer; end;
     Str80 = string[80];
{$I DMVSYN.INC}

const
  { Kanon "Bruder Jakob": Halbtoene ueber C und Laenge in Achteln }
  MelN = 32;
  MelP: array[1..MelN] of integer =
    (0, 2, 4, 0,  0, 2, 4, 0,  4, 5, 7,  4, 5, 7,
     7, 9, 7, 5, 4, 0,  7, 9, 7, 5, 4, 0,  0, -5, 0,  0, -5, 0);
  MelL: array[1..MelN] of integer =
    (2, 2, 2, 2,  2, 2, 2, 2,  2, 2, 4,  2, 2, 4,
     1, 1, 1, 1, 2, 2,  1, 1, 1, 1, 2, 2,  2, 2, 4,  2, 2, 4);
  Eighth = 170;                         { Achtel = 170 x 1,28 ms = 0,22 s }
  { Vibrato: Sinus mit 16 Stuetzstellen }
  Vib: array[0..15] of integer =
    (0, 23, 42, 55, 60, 55, 42, 23, 0, -23, -42, -55, -60, -55, -42, -23);
  { Arpeggio: Akkordtoene (Index in NoteInc) und Basston }
  ArpT: array[1..5, 1..3] of integer =
    ((29, 33, 36), (26, 29, 33), (22, 26, 29), (24, 28, 31), (29, 33, 36));
  ArpB: array[1..5] of integer = (5, 2, 10, 0, 5);

var
  NoteInc: array[0..48] of integer;     { Schrittweite, Index 0 = G2 (98 Hz) }
  Now, Last: integer;                   { Uhr in 1,28-ms-Schritten }
  Stop: boolean;

{ ---------------- Zeit ---------------- }

procedure Tick;                         { Uhr nachfuehren, oefter als alle 20 ms }
var V: integer;
begin
  V := SynClock;
  Now := Now + ((V - Last) and 15);
  Last := V;
end;

procedure ClockStart;
begin
  Now := 0;
  Last := SynClock;
end;

procedure WaitUntil(T: integer);
begin
  Tick;
  while (Now < T) and SynRunning do Tick;
end;

procedure Pause(N: integer);
begin
  WaitUntil(Now + N);
end;

procedure Quiet;
begin
  SynInc(1, 0); SynInc(2, 0); SynInc(3, 0); SynNoise(false);
end;

procedure CheckEsc;
var C: char;
begin
  if KeyPressed then
  begin
    Read(Kbd, C);
    if C = #27 then Stop := true;
  end;
end;

function Next(Title: Str80): boolean;   { Ueberschrift; false = ESC }
begin
  Quiet;
  CheckEsc;
  if not SynRunning then
    if not SynBegin then Stop := true;
  if (not Stop) and (Title <> '') then WriteLn(Title);
  Next := not Stop;
end;

procedure MakeNotes;
var I: integer;
    F: real;
begin
  for I := 0 to 48 do
  begin
    F := 97.999 * Exp(I * Ln(2.0) / 12.0);
    NoteInc[I] := Round(F * 65536.0 / SynLoopHz);
  end;
end;

{ ---------------- Teile ---------------- }

procedure Scale;
const S: array[1..8] of integer = (17, 19, 21, 22, 24, 26, 28, 29);
var I: integer;
begin
  ClockStart;
  for I := 1 to 8 do
  begin
    SynInc(1, NoteInc[S[I]]);
    WaitUntil(I * 150);
  end;
end;

procedure Chord;
var I: integer;
begin
  SynInc(1, NoteInc[17]); SynInc(2, NoteInc[21]); SynInc(3, NoteInc[24]);
  ClockStart;
  for I := 0 to 3 do
  begin
    SynNoise(true);  WaitUntil(I * 240 + 40);
    SynNoise(false); WaitUntil(I * 240 + 240);
  end;
end;

procedure Arpeggio;
var P, K, C, T: integer;
begin
  { a) ein Akkord, immer schneller gebrochen, bis das Ohr drei Toene hoert }
  ClockStart;
  P := 120; K := 0;
  while P > 10 do
  begin
    SynInc(1, NoteInc[ArpT[1, K + 1]]);
    K := (K + 1) mod 3;
    Pause(P);
    P := P - (P + 15) div 16;
  end;
  T := Now + 700;
  while Now < T do
  begin
    SynInc(1, NoteInc[ArpT[1, K + 1]]);
    K := (K + 1) mod 3;
    Pause(10);
  end;
  SynInc(1, 0);
  Pause(250);
  { b) Akkordfolge C - a - F - G - C mit Bass, eine Stimme bricht die Akkorde }
  for C := 1 to 5 do
  begin
    SynInc(2, NoteInc[ArpB[C]]);
    T := Now + 700;
    if C = 5 then T := Now + 1200;
    while Now < T do
    begin
      SynInc(1, NoteInc[ArpT[C, K + 1]]);
      K := (K + 1) mod 3;
      Pause(12);
    end;
  end;
end;

procedure Siren;
var I, J, X: integer;
begin
  WriteLn('   Sirene');
  ClockStart;
  X := 3900;                             { 400 Hz }
  for I := 1 to 3 do
  begin
    for J := 1 to 120 do begin X := X + 65; SynInc(1, X); Pause(2); end;
    for J := 1 to 120 do begin X := X - 65; SynInc(1, X); Pause(2); end;
  end;
  SynInc(1, 0);
end;

procedure Vibrato;
var I, A: integer;
begin
  WriteLn('   Vibrato');
  ClockStart;
  A := NoteInc[26];                      { a' = 440 Hz }
  SynInc(1, A);
  Pause(450);
  for I := 0 to 191 do
  begin
    SynInc(1, A + Vib[I and 15]);
    Pause(8);                            { 16 x 8 Schritte = 6 Hz }
  end;
  SynInc(1, 0);
end;

procedure Beat;
var A: integer;
begin
  WriteLn('   Schwebung: 440 Hz gegen 442 Hz, dann drei Stimmen');
  A := NoteInc[26];
  ClockStart;
  SynInc(1, A); SynInc(2, A + 19);       { +2 Hz }
  Pause(1600);
  SynInc(3, A - 29);                     { -3 Hz }
  Pause(1600);
  SynInc(1, 0); SynInc(2, 0); SynInc(3, 0);
end;

procedure Laser;
var I, X: integer;
begin
  WriteLn('   Laser');
  ClockStart;
  for I := 1 to 6 do
  begin
    X := 19000;                          { 1950 Hz }
    while X > 1500 do
    begin
      SynInc(1, X);
      X := X - X div 16;
      Pause(2);
    end;
    SynInc(1, 0);
    Pause(60 + 40 * (I and 1));
  end;
end;

procedure Explosion;
var Off, X, S: integer;
begin
  WriteLn('   Explosion');
  ClockStart;
  X := 1200;                             { Grollen ab 123 Hz abwaerts }
  Off := 0; S := $A5;
  while Off < 60 do
  begin
    SynReg($1F, S);                      { Rauschen ein, jedesmal anderer Start }
    S := (S * 5 + 3) and 255;
    if S = 0 then S := 1;
    SynInc(1, X);
    Pause(10);
    SynNoise(false);
    Pause(Off);
    Off := Off + 3;
    X := X - X div 12;
  end;
  SynInc(1, 0);
end;

procedure Canon;
var E, V, L, T, Burst: integer;
    Ptr, NextS: array[1..3] of integer;
const
  Base: array[1..3] of integer = (29, 17, 5);  { C5, C4, C3 }
begin
  for V := 1 to 3 do begin Ptr[V] := 1; NextS[V] := 0; end;
  ClockStart;
  E := 0;
  while (E < 96) and not Stop do
  begin
    T := E * Eighth;
    WaitUntil(T);
    { Toene anschlagen: Stimme V setzt nach 2 (V-1) Takten ein }
    for V := 1 to 3 do
    begin
      L := E - 16 * (V - 1);
      if (L >= 0) and (L < 64) then
        if L = NextS[V] then
        begin
          SynInc(V, NoteInc[Base[V] + MelP[Ptr[V]]]);
          NextS[V] := NextS[V] + MelL[Ptr[V]];
          Ptr[V] := Ptr[V] + 1;
        end;
    end;
    { Schlagzeug aus Rauschen: jedes Viertel kurz, Taktanfang laenger }
    if (E and 1) = 0 then
    begin
      if (E and 7) = 0 then Burst := 14 else Burst := 4;
      SynNoise(true);
      WaitUntil(T + Burst);
      SynNoise(false);
    end;
    { kurz vor Ende des Achtels Toene absetzen, die jetzt enden }
    WaitUntil(T + Eighth - 20);
    for V := 1 to 3 do
    begin
      L := E - 16 * (V - 1);
      if (L >= 0) and (L < 64) then
        if L + 1 = NextS[V] then SynInc(V, 0);
    end;
    CheckEsc;
    E := E + 1;
  end;
  WaitUntil(96 * Eighth);
end;

begin
  WriteLn('SYNDEMO - Synthesizer im Tastatur-Controller der DMV');
  if not SynAvail then
  begin
    WriteLn('Keine Synthesizer-Firmware im 8741.');
    Halt;
  end;
  if not SynBegin then begin WriteLn('Synthesizer startet nicht.'); Halt; end;
  WriteLn('(ESC beendet)');
  MakeNotes;
  Stop := false;
  if Next('Tonleiter') then Scale;
  if Next('Akkord C-E-G, dazu Rauschen') then Chord;
  if Next('Arpeggio: eine Stimme, immer schneller gebrochen; dann mit Bass') then Arpeggio;
  if Next('Klangeffekte') then Siren;
  if Next('') then Vibrato;
  if Next('') then Beat;
  if Next('') then Laser;
  if Next('') then Explosion;
  if Next('Kanon "Bruder Jakob", drei Stimmen und Schlagzeug') then Canon;
  Quiet;
  SynEnd;
  WriteLn('Ende.');
end.
