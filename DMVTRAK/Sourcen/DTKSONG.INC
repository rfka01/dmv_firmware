{ DTKSONG.INC - Lieddaten

  Zelle = 4 Byte: Note (0 leer, 1..60 = C-0..H-4, MOD C-1 = 13),
  Instrument (0..31), Effekt (0..15), Parameter.
  Pattern p, Zeile r, Kanal c: Pat[p]^[(r * NChan + c) * 4 + k]. }

const
  PerTab: array[1..60] of integer = (
    1712,1616,1524,1440,1356,1280,1208,1140,1076,1016, 960, 906,
     856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
     428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
     214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
     107, 101,  95,  90,  85,  80,  75,  71,  67,  63,  60,  56);
  NoteNm: string[24] = 'C-C#D-D#E-F-F#G-G#A-A#H-';
  NoteOfs = -9;          { MOD C-1 (13) -> 8741-Ton 4 = C 131 Hz }

var
  LastErr : Str80;

{ Ganzzahl-Hilfen (8088-Befehle DIV/MUL): Gleitkomma in Turbo Pascal 3
  braucht auf dem 8088 je Rechnung Millisekunden - zu langsam im Tick. }

procedure UDiv(H, L, D: integer; var Q: integer);   { Q = (H:L) / D, H < D }
begin
  Inline(
    $8B/$56/<H/             {  mov  dx,[bp+H]     }
    $8B/$46/<L/             {  mov  ax,[bp+L]     }
    $F7/$76/<D/             {  div  word [bp+D]   }
    $C4/$7E/<Q/             {  les  di,[bp+Q]     }
    $26/$89/$05);           {  mov  es:[di],ax    }
end;

procedure UMul(A, B: integer; var H, L: integer);  { H:L = A * B }
begin
  Inline(
    $8B/$46/<A/             {  mov  ax,[bp+A]     }
    $F7/$66/<B/             {  mul  word [bp+B]   }
    $C4/$7E/<L/             {  les  di,[bp+L]     }
    $26/$89/$05/            {  mov  es:[di],ax    }
    $C4/$7E/<H/             {  les  di,[bp+H]     }
    $26/$89/$15);           {  mov  es:[di],dx    }
end;

function Real16(R: real): integer;   { 0..65535 als vorzeichenlose Zahl }
begin
  if R >= 32768.0 then Real16 := Trunc(R - 65536.0) else Real16 := Trunc(R);
end;

function PerToNote(P: integer): integer;   { naechstliegende Note }
var I, Best, D, BD: integer;
begin
  Best := 1; BD := 32000;
  for I := 1 to 60 do
  begin
    D := Abs(PerTab[I] - P);
    if D < BD then begin BD := D; Best := I; end;
  end;
  PerToNote := Best;
end;

function NoteName(N: integer): Str20;
begin
  if N = 0 then
    NoteName := '---'
  else
    NoteName := Copy(NoteNm, ((N - 1) mod 12) * 2 + 1, 2) +
                Chr(Ord('0') + (N - 1) div 12);
end;

function PatSize: integer;
begin
  PatSize := Rows * NChan * 4;
end;

function AllocPat(P: integer): boolean;    { leeres Pattern anlegen }
var Need: integer;
begin
  AllocPat := true;
  if Pat[P] <> nil then exit;
  Need := PatSize div 16 + 2;
  if (MaxAvail >= 0) and (MaxAvail < Need) then
  begin
    LastErr := 'Kein Speicher mehr fuer Pattern ' + IStr(P);
    AllocPat := false;
    exit;
  end;
  GetMem(Pat[P], PatSize);
  FillChar(Pat[P]^, PatSize, 0);
  if P >= NPat then NPat := P + 1;
end;

procedure FreeSong;
var I: integer;
begin
  for I := 0 to MaxPat - 1 do
    if Pat[I] <> nil then
    begin
      FreeMem(Pat[I], PatSize);
      Pat[I] := nil;
    end;
  NPat := 0;
end;

procedure InsDefault(I: integer);
begin
  IName[I] := '';
  IPrio[I] := 5;
  ITrans[I] := 0;
  IVol[I] := 64;
  IHold[I] := true;
  ILen[I] := 0;
  INoise[I] := false;
end;

procedure NewSong;
var I: integer;
begin
  FreeSong;
  NChan := 4;
  Title := 'neu';
  SongLen := 1;
  Restart := 0;
  FillChar(Order, SizeOf(Order), 0);
  for I := 1 to NIns do InsDefault(I);
  ArpMode := false;
  GTrans := 0;
  InitSpd := 6;
  InitBPM := 125;
  Looping := true;
  FileName := '';
  Modified := false;
  if AllocPat(0) then ;
end;

function CellB(P, R, C, K: integer): integer;
begin
  if Pat[P] = nil then CellB := 0
  else CellB := Pat[P]^[(R * NChan + C) * 4 + K];
end;

procedure SetCell(P, R, C, K, V: integer);
begin
  if Pat[P] = nil then
    if not AllocPat(P) then exit;
  Pat[P]^[(R * NChan + C) * 4 + K] := V;
  Modified := true;
end;

function CellText(P, R, C: integer): Str20;   { 'C-2 01 A0F' }
const HexC: string[16] = '0123456789ABCDEF';
var N, I, E, A, B: integer;
    S: Str20;
begin
  N := 0; I := 0; E := 0; A := 0;
  if Pat[P] <> nil then
  begin
    B := (R * NChan + C) * 4;
    N := Pat[P]^[B]; I := Pat[P]^[B + 1];
    E := Pat[P]^[B + 2]; A := Pat[P]^[B + 3];
  end;
  S[0] := Chr(10);                { Laenge direkt setzen: keine Verkettung }
  if N = 0 then
  begin S[1] := '-'; S[2] := '-'; S[3] := '-'; end
  else
  begin
    B := ((N - 1) mod 12) * 2;
    S[1] := NoteNm[B + 1]; S[2] := NoteNm[B + 2];
    S[3] := Chr(48 + (N - 1) div 12);
  end;
  S[4] := ' ';
  if I = 0 then begin S[5] := '.'; S[6] := '.'; end
  else begin S[5] := HexC[(I shr 4) + 1]; S[6] := HexC[(I and 15) + 1]; end;
  S[7] := ' ';
  if (E = 0) and (A = 0) then begin S[8] := '.'; S[9] := '.'; S[10] := '.'; end
  else
  begin
    S[8] := HexC[E + 1]; S[9] := HexC[(A shr 4) + 1]; S[10] := HexC[(A and 15) + 1];
  end;
  CellText := S;
end;

{ 8741-Ton aus Note und Instrument, in 1..42 gefaltet }
function ToneOf(N, I: integer): integer;
var T: integer;
begin
  T := N + NoteOfs + GTrans;
  if (I >= 1) and (I <= NIns) then T := T + ITrans[I];
  while T < 1 do T := T + 12;
  while T > 42 do T := T - 12;
  ToneOf := T;
end;
