{ DTKPLAY.INC - Abspielen

  Wie ein MOD-Player mit Ticks (Standard 125 BPM = 20 ms je Tick).
  Jeder Tick ergibt genau einen Ton fuer den Lautsprecher:
  - fuehrende Stimme: klingender Kanal mit der hoechsten Instrument-
    Prioritaet, bei Gleichstand der zuletzt angeschlagene;
  - Arpeggio: die klingenden Kanaele wechseln sich je Tick ab.
  Gleiche Toene hintereinander werden zusammengefasst. Die Tickzeit
  wird auf 20,48-ms-Einheiten des 8741 umgerechnet, der Rest wird
  mitgenommen, damit das Tempo im Mittel stimmt.
  Unterstuetzte Effekte: 0 Arpeggio, 1/2 Slide, 3 Tonportamento,
  5/6/A Lautstaerke-Slide, 9 Sample-Offset (verkuerzt die Dauer),
  B Sprung, C Lautstaerke (0 = aus), D Pattern-Ende, F Speed/BPM,
  E1/E2 Feinslide, E6 Schleife, EA/EB Feinlautstaerke, EC Abschneiden,
  ED Verzoegern, EE Pattern-Pause. 4/7 (Vibrato, Tremolo) entfallen. }

var
  Playing, PlEnd, PlOnce : boolean;
  PlO, PlR, PlTick       : integer;
  Speed, BPM, TickLen    : integer;
  PDelay                 : integer;
  BrkO, BrkR             : integer;
  LoopRow, LoopCnt       : integer;
  LoopJmp                : boolean;
  CPer, CIns, CVol, CRem : array[0..7] of integer;
  CTrig, CEff, CPar      : array[0..7] of integer;
  CTgt, CPSpd            : array[0..7] of integer;
  CDlyT, CDlyN, CDlyI    : array[0..7] of integer;
  CNote, CNPer           : array[0..7] of integer;   { Notenpuffer }
  TrigCnt, ArpIdx        : integer;
  Acc, PendT, PendU      : integer;
  PendO, PendR           : integer;
  TickLen80              : integer;    { Ticklaenge in 80 us (Synthesizer) }
  DurK                   : array[1..31] of integer;
  Dur256                 : integer;
  SynMode                : boolean;    { spielt gerade ueber den Synthesizer }
  UseSyn                 : boolean;    { Synthesizer benutzen, wenn vorhanden }

procedure SynTick; forward;
function SynStart: boolean; forward;
procedure SynStop; forward;
procedure SynService; forward;

function NoteOfPer(P: integer): integer;   { binaere Suche, Tabelle fallend }
var L, H, M: integer;
begin
  if P >= PerTab[1] then begin NoteOfPer := 1; exit; end;
  if P <= PerTab[60] then begin NoteOfPer := 60; exit; end;
  L := 1; H := 60;
  while H - L > 1 do
  begin
    M := (L + H) div 2;
    if PerTab[M] >= P then L := M else H := M;
  end;
  if PerTab[L] - P <= P - PerTab[H] then NoteOfPer := L else NoteOfPer := H;
end;

procedure SetDurK;               { Dauer-Faktoren fuer das aktuelle BPM }
var I: integer;
    R: real;
begin
  for I := 1 to NIns do
  if ILen[I] < 1.0 then DurK[I] := 0 else
  begin
    R := ILen[I] * BPM / 8867236.5 * 4096.0;
    if R > 32000.0 then R := 32000.0;
    DurK[I] := Round(R);
  end;
  Dur256 := Round(256.0 * BPM / 8867236.5 * 4096.0);
end;

procedure SetBPM(B: integer);
begin
  if B < 32 then B := 32;
  BPM := B;
  TickLen := Trunc(250000.0 / B);   { 1/100 ms }
  TickLen80 := Round(31250.0 / B);  { 80 us = ein 8741-Timerschritt }
  SetDurK;
end;

{ Dauer des Samples in Ticks: Laenge * Periode * BPM / 8867236,5.
  DurK = Laenge * BPM / 8867236,5 * 4096, also (DurK * Periode) / 4096. }
function DurTicks(C, Ofs: integer): integer;
var I, H, L, D, H2, L2: integer;
begin
  I := CIns[C];
  if (I < 1) or (I > NIns) then begin DurTicks := 0; exit; end;
  if IHold[I] then begin DurTicks := -1; exit; end;
  UMul(DurK[I], CPer[C], H, L);
  D := H * 16 + (L shr 12);
  if Ofs > 0 then
  begin
    UMul(Ofs * Dur256, CPer[C], H2, L2);
    D := D - (H2 * 16 + (L2 shr 12));
  end;
  if D > 30000 then D := 30000;
  if D < 1 then D := 1;
  DurTicks := D;
end;

procedure Trig(C, N, I: integer);
begin
  if I > 0 then
  begin
    CIns[C] := I;
    CVol[C] := IVol[I];
  end;
  if N > 0 then
  begin
    CPer[C] := PerTab[N];
    CRem[C] := DurTicks(C, 0);
    if TrigCnt > 30000 then TrigCnt := 0;
    TrigCnt := TrigCnt + 1;
    CTrig[C] := TrigCnt;
  end;
end;

procedure ClampCh(C: integer);
begin
  if CPer[C] < 56 then CPer[C] := 56;
  if CPer[C] > 1712 then CPer[C] := 1712;
  if CVol[C] < 0 then CVol[C] := 0;
  if CVol[C] > 64 then CVol[C] := 64;
end;

procedure DoRow;
var C, P, N, I, E, A, X, Y: integer;
begin
  P := Order[PlO];
  PDelay := 0;
  for C := 0 to NChan - 1 do
  begin
    N := CellB(P, PlR, C, 0); I := CellB(P, PlR, C, 1);
    E := CellB(P, PlR, C, 2); A := CellB(P, PlR, C, 3);
    X := A shr 4; Y := A and 15;
    CEff[C] := E; CPar[C] := A;
    CDlyT[C] := -1;
    if (E = $E) and (X = $D) and (Y > 0) then
    begin
      CDlyT[C] := Y; CDlyN[C] := N; CDlyI[C] := I;
    end
    else if (E = 3) or (E = 5) then
    begin
      if I > 0 then begin CIns[C] := I; CVol[C] := IVol[I]; end;
      if N > 0 then CTgt[C] := PerTab[N];
      if (E = 3) and (A > 0) then CPSpd[C] := A;
    end
    else if (N > 0) or (I > 0) then
      Trig(C, N, I);
    case E of
      9: if (N > 0) and (CRem[C] > 0) then
         begin
           CRem[C] := DurTicks(C, A);
         end;
      $B: begin BrkO := A; if BrkR < 0 then BrkR := 0; end;
      $C: CVol[C] := A;
      $D: begin
            if BrkO < 0 then BrkO := PlO + 1;
            BrkR := X * 10 + Y;
            if BrkR > 63 then BrkR := 0;
          end;
      $E: case X of
            1: CPer[C] := CPer[C] - Y;
            2: CPer[C] := CPer[C] + Y;
            6: if Y = 0 then LoopRow := PlR
               else
               begin
                 if LoopCnt = 0 then
                 begin LoopCnt := Y; LoopJmp := true; end
                 else
                 begin
                   LoopCnt := LoopCnt - 1;
                   if LoopCnt > 0 then LoopJmp := true;
                 end;
               end;
            $A: CVol[C] := CVol[C] + Y;
            $B: CVol[C] := CVol[C] - Y;
            $C: if Y = 0 then CVol[C] := 0;
            $E: PDelay := Y;
          end;
      $F: if A = 0 then PlEnd := true
          else if A < 32 then Speed := A
          else SetBPM(A);
    end;
    ClampCh(C);
    PitTrack;
  end;
end;

procedure DoTickFx(T: integer);
var C, A, X, Y: integer;
begin
  for C := 0 to NChan - 1 do
  begin
    A := CPar[C]; X := A shr 4; Y := A and 15;
    case CEff[C] of
      1: CPer[C] := CPer[C] - A;
      2: CPer[C] := CPer[C] + A;
      3, 5: if CTgt[C] > 0 then
            begin
              if CPer[C] < CTgt[C] then
              begin
                CPer[C] := CPer[C] + CPSpd[C];
                if CPer[C] > CTgt[C] then CPer[C] := CTgt[C];
              end
              else if CPer[C] > CTgt[C] then
              begin
                CPer[C] := CPer[C] - CPSpd[C];
                if CPer[C] < CTgt[C] then CPer[C] := CTgt[C];
              end;
            end;
    end;
    if (CEff[C] = 5) or (CEff[C] = 6) or (CEff[C] = $A) then
      if X > 0 then CVol[C] := CVol[C] + X else CVol[C] := CVol[C] - Y;
    if CEff[C] = $E then
    begin
      if (X = $C) and (T = Y) then CVol[C] := 0;
    end;
    if CDlyT[C] = T then Trig(C, CDlyN[C], CDlyI[C]);
    ClampCh(C);
    if SynMode then PitTrack;
  end;
end;

function ChTone(C: integer): integer;
var N, A: integer;
begin
  if CPer[C] <> CNPer[C] then
  begin
    CNPer[C] := CPer[C];
    CNote[C] := NoteOfPer(CPer[C]);
  end;
  N := CNote[C];
  A := CPar[C];
  if (CEff[C] = 0) and (A > 0) then
    case PlTick mod 3 of
      1: N := N + A shr 4;
      2: N := N + A and 15;
    end;
  ChTone := ToneOf(N, CIns[C]);
end;

function Sounding(C: integer): boolean;
begin
  Sounding := (CPer[C] > 0) and (CIns[C] > 0) and (CVol[C] > 0)
              and (CRem[C] <> 0) and (IPrio[CIns[C]] > 0);
end;

function SelectTone: integer;
var C, Best, N, K: integer;
    Cand: array[0..7] of integer;
begin
  Best := -1; N := 0;
  for C := 0 to NChan - 1 do
    if Sounding(C) then
    begin
      Cand[N] := C; N := N + 1;
      if Best < 0 then Best := C
      else if (IPrio[CIns[C]] > IPrio[CIns[Best]]) or
              ((IPrio[CIns[C]] = IPrio[CIns[Best]]) and
               (CTrig[C] > CTrig[Best])) then Best := C;
    end;
  if N = 0 then SelectTone := 0
  else if ArpMode then
  begin
    K := ArpIdx mod N;
    ArpIdx := ArpIdx + 1;
    if ArpIdx > 30000 then ArpIdx := 0;
    SelectTone := ChTone(Cand[K]);
  end
  else
    SelectTone := ChTone(Best);
end;

procedure FlushPend;
begin
  if PendU > 0 then Tone2(PendT, PendU, PendO, PendR);
  PendU := 0;
end;

procedure Emit(T: integer);
var U: integer;
begin
  Acc := Acc + TickLen;
  U := Acc div Unit8741;
  Acc := Acc mod Unit8741;
  if U = 0 then exit;
  if (PendU > 0) and (T = PendT) then
    PendU := PendU + U
  else
  begin
    FlushPend;
    PendT := T; PendU := U; PendO := PlO; PendR := PlR;
  end;
  while PendU >= MaxLen do
  begin
    Tone2(PendT, MaxLen, PendO, PendR);
    PendU := PendU - MaxLen;
  end;
end;

procedure NextRow;
begin
  PlTick := 0;
  if LoopJmp then
  begin
    LoopJmp := false;
    PlR := LoopRow;
  end
  else if (BrkO >= 0) or (BrkR >= 0) then
  begin
    if BrkO >= 0 then PlO := BrkO else PlO := PlO + 1;
    if BrkR >= 0 then PlR := BrkR else PlR := 0;
    BrkO := -1; BrkR := -1;
    LoopRow := 0; LoopCnt := 0;
  end
  else
  begin
    PlR := PlR + 1;
    if PlR >= Rows then
    begin
      PlR := 0; PlO := PlO + 1;
      LoopRow := 0; LoopCnt := 0;
    end;
  end;
  if PlO >= SongLen then
  begin
    if Looping and not PlOnce then
    begin
      if Restart < SongLen then PlO := Restart else PlO := 0;
    end
    else
      PlEnd := true;
  end;
end;

procedure GenTick;
var C: integer;
begin
  if PlEnd then exit;
  if PlTick = 0 then DoRow else DoTickFx(PlTick mod Speed);
  if PlEnd then exit;
  if PlTick = 0 then
    if SynMode then begin NowO := PlO; NowR := PlR; NowNew := true; end;
  if SynMode then begin PitTrack; SynTick; end else Emit(SelectTone);
  for C := 0 to NChan - 1 do
    if CRem[C] > 0 then CRem[C] := CRem[C] - 1;
  PlTick := PlTick + 1;
  if Speed < 1 then Speed := 1;
  if PlTick >= Speed * (1 + PDelay) then NextRow;
end;

procedure PlayStart(O, R: integer; Once: boolean);
var C: integer;
begin
  SndClear;
  for C := 0 to 7 do
  begin
    CPer[C] := 0; CIns[C] := 0; CVol[C] := 0; CRem[C] := 0;
    CTrig[C] := 0; CEff[C] := 0; CPar[C] := 0; CTgt[C] := 0;
    CPSpd[C] := 0; CDlyT[C] := -1; CNPer[C] := -1; CNote[C] := 1;
  end;
  Speed := InitSpd; if Speed < 1 then Speed := 6;
  SetBPM(InitBPM);
  if O >= SongLen then O := 0;
  PlO := O; PlR := R; PlTick := 0; PDelay := 0;
  BrkO := -1; BrkR := -1; LoopRow := 0; LoopCnt := 0; LoopJmp := false;
  TrigCnt := 0; ArpIdx := 0;
  Acc := 0; PendU := 0; PendT := 0;
  PlEnd := false; PlOnce := Once;
  SynMode := false;
  if UseSyn then SynMode := SynStart;
  Playing := true;
  NowO := O; NowR := R; NowNew := true;
end;

procedure PlayStop;
begin
  if SynMode then SynStop;
  SynMode := false;
  SndClear;
  PlEnd := true;
  PendU := 0;
  Playing := false;
end;

procedure PlayService;           { im Hauptschleifen-Takt aufrufen }
var N: integer;
begin
  Pump;
  if not Playing then exit;
  if SynMode then
  begin
    SynService;
    if PlEnd then
    begin
      SynStop;
      SynMode := false;
      Playing := false;
    end;
    exit;
  end;
  N := 0;
  while (not PlEnd) and (SqCount < 12) and (N < 4) do
  begin
    GenTick;
    N := N + 1;
  end;
  if PlEnd then
  begin
    FlushPend;
    if not SndBusy then Playing := false;
  end;
end;

procedure Service;
begin
  PlayService;
end;
