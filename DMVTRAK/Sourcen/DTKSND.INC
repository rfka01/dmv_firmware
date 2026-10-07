{ DTKSND.INC - Tonausgabe ueber den Tastatur-Controller 8741

  Befehl 06h an Port 41h, dann Ton+20h und Laenge+1Fh an Port 40h.
  Ton n = 110 * 2^((n-1)/12) Hz (1..42), 0 = Pause; Laenge in 20,48 ms.
  Vor jedem Schreiben muss Bit 1 (IBF) von Port 41h frei sein.
  Der 8741 spielt den Ton selbst. Ein Befehl fuer den naechsten Ton
  bleibt in seinem Eingangspuffer liegen, bis der laufende Ton fertig
  ist - der 8741 ist damit der Taktgeber. Sobald er den Befehl abholt,
  beginnt der naechste Ton: Daran haengt die Positionsanzeige. }

var
  SqT, SqL : array[0..31] of byte;
  SqO, SqR : array[0..31] of byte;        { Liedposition beim Tonbeginn }
  SqHead, SqTail, SqPh : integer;
  NowO, NowR : integer;                   { gerade klingende Position }
  NowNew     : boolean;                   { Position hat gewechselt }

procedure SndInit;
begin
  SqHead := 0; SqTail := 0; SqPh := 0;
  NowO := 0; NowR := 0; NowNew := false;
end;

function SqCount: integer;
begin
  SqCount := (SqTail - SqHead + QSize) mod QSize;
end;

procedure Pump;
var K: integer;
begin
  if SqPh = 0 then
  begin
    if SqHead = SqTail then exit;
    if (Port[$41] and 2) <> 0 then exit;
    Port[$41] := $06;
    SqPh := 1;
  end;
  if SqPh = 1 then
  begin
    if (Port[$41] and 2) <> 0 then exit;   { voriger Ton laeuft noch }
    Port[$40] := SqT[SqHead] + $20;
    NowO := SqO[SqHead]; NowR := SqR[SqHead]; NowNew := true;
    SqPh := 2;
  end;
  if SqPh = 2 then
  begin
    K := 0;
    while ((Port[$41] and 2) <> 0) and (K < 200) do K := K + 1;
    if (Port[$41] and 2) <> 0 then exit;
    Port[$40] := SqL[SqHead] + $1F;
    SqHead := (SqHead + 1) mod QSize;
    SqPh := 0;
  end;
end;

procedure Tone2(N, L, O, R: integer);   { in die Warteschlange }
begin
  if ((SqTail + 1) mod QSize) = SqHead then exit;
  if L < 1 then L := 1;
  if L > 224 then L := 224;
  if (N < 0) or (N > 42) then N := 0;
  SqT[SqTail] := N; SqL[SqTail] := L;
  SqO[SqTail] := O; SqR[SqTail] := R;
  SqTail := (SqTail + 1) mod QSize;
end;

procedure Tone(N, L: integer);
begin
  Tone2(N, L, 0, 0);
end;

procedure SndClear;                    { Wartendes verwerfen }
begin
  if SqPh = 0 then
    SqTail := SqHead
  else
    SqTail := (SqHead + 1) mod QSize;  { angefangenen Befehl zu Ende senden }
end;

function SndBusy: boolean;
begin
  SndBusy := (SqHead <> SqTail) or (SqPh <> 0);
end;

procedure SndIdle;                     { alles senden }
var K: integer;
begin
  K := 0;
  while SndBusy and (K < 30000) do
  begin
    Pump;
    K := K + 1;
  end;
end;

procedure Service; forward;      { Pump + Synthesizer-Takt, in DTKPLAY }
