{ DTKSCR.INC - Bildschirm (ADM-3A ueber DOS) und Tastatur

  Die DMV-Konsole versteht ADM-3A: ESC = Zeile+32 Spalte+32, ^Z loescht.
  Gezeichnet wird in ein Abbild (Want); Flush schreibt nur, was sich
  gegenueber dem Bildschirm (Have) geaendert hat, und laesst zwischen
  den Zeichen die Tonwarteschlange weiterlaufen. Spalte 79 bleibt frei,
  damit die Konsole nie umbricht oder rollt. }

const
  KUp = 1001; KDown = 1002; KLeft = 1003; KRight = 1004;
  KPgUp = 1005; KPgDn = 1006; KHome = 1007; KEnd = 1008; KDel = 1009;
  KEsc = 27; KRet = 13; KTab = 9;

var
  Want, Have : array[0..23] of Str80;
  CurX, CurY : integer;

var
  ConRegs : RegPack;

procedure ConOut(C: char);       { DOS 06h: ein Zeichen, ohne Umweg ueber Write }
begin
  ConRegs.AX := $0600;
  ConRegs.DX := Ord(C);
  MsDos(ConRegs);
end;

procedure GoXY(X, Y: integer);
begin
  ConOut(#27); ConOut('='); ConOut(Chr(32 + Y)); ConOut(Chr(32 + X));
end;

procedure ScrReset;
var I: integer;
begin
  ConOut(#26);
  for I := 0 to ScrRows - 1 do Have[I] := '';
end;

procedure ScrBlank;
var I: integer;
begin
  for I := 0 to ScrRows - 1 do Want[I] := '';
end;

procedure Put(X, Y: integer; S: Str80);     { in das Abbild schreiben }
var I, L, N: integer;
begin
  if (Y < 0) or (Y >= ScrRows) then exit;
  N := X + Length(S);
  if N > 79 then N := 79;
  L := Length(Want[Y]);
  if L < N then                 { auffuellen, Laenge direkt setzen }
  begin
    for I := L + 1 to N do Want[Y][I] := ' ';
    Want[Y][0] := Chr(N);
  end;
  for I := 1 to N - X do Want[Y][X + I] := S[I];
end;

procedure PutLine(Y: integer; S: Str80);
begin
  if Length(S) > 79 then S := Copy(S, 1, 79);
  Want[Y] := S;
end;

function ChAt(var S: Str80; I: integer): char;
begin
  if I <= Length(S) then ChAt := S[I] else ChAt := ' ';
end;

procedure PitTrack; forward;      { Zeitmessung (DTKSYN), alle paar ms aufrufen }

function DisplayStale: boolean; forward;   { in DTKEDIT: Seite veraltet? }

{ Schreibt nur die Zeichen, die sich gegenueber dem Bildschirm aendern.
  Gleiche Stellen werden uebersprungen: bis 4 Zeichen werden mitgeschrieben,
  bei laengeren Strecken setzt GoXY (4 Zeichen) den Cursor neu. Ist die
  angezeigte Seite beim Abspielen veraltet, bricht Flush ab; Have wird
  dabei zeichengenau nachgefuehrt. }
procedure Flush;
var Y, I, A, B, L, LW, LH, Col: integer;
    CW, CH: char;
    Diff, Stop: boolean;
begin
  Stop := false;
  for Y := 0 to ScrRows - 1 do
  if not Stop then
  begin
    PitTrack;
    if Want[Y] <> Have[Y] then
    begin
      LW := Length(Want[Y]); LH := Length(Have[Y]);
      if LW > LH then L := LW else L := LH;
      { erste und letzte abweichende Spalte; Zeichen hinter dem Ende = ' ' }
      A := 1; Diff := false;
      while (A <= L) and not Diff do
      begin
        if A <= LW then CW := Want[Y][A] else CW := ' ';
        if A <= LH then CH := Have[Y][A] else CH := ' ';
        if CW <> CH then Diff := true else A := A + 1;
        if (A and 3) = 0 then PitTrack;
      end;
      B := L; Diff := false;
      while (B >= A) and not Diff do
      begin
        if B <= LW then CW := Want[Y][B] else CW := ' ';
        if B <= LH then CH := Have[Y][B] else CH := ' ';
        if CW <> CH then Diff := true else B := B - 1;
        if (B and 3) = 0 then PitTrack;
      end;
      Col := 0;                       { Spalte, an der der Cursor steht }
      I := A;
      while (I <= B) and not Stop do
      begin
        if I <= LW then CW := Want[Y][I] else CW := ' ';
        if I <= LH then CH := Have[Y][I] else CH := ' ';
        if CW <> CH then
        begin
          if Col <> I then
          begin
            if (Col > 0) and (I - Col <= 4) then
              while Col < I do            { kurze gleiche Strecke mitschreiben }
              begin
                if Col <= LW then ConOut(Want[Y][Col]) else ConOut(' ');
                Col := Col + 1;
              end
            else
              GoXY(I - 1, Y);
          end;
          ConOut(CW);
          Col := I + 1;
          Service;
          if DisplayStale then Stop := true;
        end
        else if (I and 3) = 0 then PitTrack;
        I := I + 1;
      end;
      if Stop then
        { geschrieben sind die Spalten bis I-1: Have entsprechend }
        Have[Y] := Copy(Want[Y], 1, I - 1) + Copy(Have[Y], I, 255)
      else
        Have[Y] := Want[Y];
      PitTrack;
    end;
  end;
  GoXY(CurX, CurY);
end;

function Hex(B, D: integer): Str20;
const H: string[16] = '0123456789ABCDEF';
var S: Str20;
begin
  S := '';
  while D > 0 do
  begin
    S := H[(B and 15) + 1] + S;
    B := B shr 4;
    D := D - 1;
  end;
  Hex := S;
end;

function Dec2(N: integer): Str20;   { zweistellig, ohne Str (schneller) }
var S: Str20;
begin
  if (N >= 0) and (N < 100) then
    Dec2 := Chr(48 + N div 10) + Chr(48 + N mod 10)
  else
  begin
    Str(N, S);
    Dec2 := S;
  end;
end;

function IStr(N: integer): Str20;
var S: Str20;
begin
  Str(N, S);
  IStr := S;
end;

function Pad(S: Str80; N: integer): Str80;
begin
  while Length(S) < N do S := S + ' ';
  Pad := Copy(S, 1, N);
end;

{ ---------------- Tastatur ----------------
  Sondertasten kommen je nach DOS als #0 + IBM-Code (DOS 3.3),
  ESC [ A..D oder als ADM-3A-Steuerzeichen (^K ^J ^H ^L).
  WordStar geht auch (^E ^X ^S ^D, ^R ^C fuer Seiten). }

procedure KbdInit;
var C: char;
begin
  while KeyPressed do Read(Kbd, C);
end;

function WaitKey(N: integer): boolean;  { kurz auf Folgezeichen warten }
var I: integer;
begin
  I := 0;
  while (I < N) and not KeyPressed do
  begin
    Service;
    I := I + 1;
  end;
  WaitKey := KeyPressed;
end;

function GetKey: integer;              { 0 = keine Taste }
var C: char;
    K: integer;
begin
  GetKey := 0;
  if not KeyPressed then exit;
  Read(Kbd, C);
  K := Ord(C);
  if C = #0 then
  begin
    K := 0;
    if WaitKey(2000) then
    begin
      Read(Kbd, C);
      case C of
        'H': K := KUp;    'P': K := KDown;
        'K': K := KLeft;  'M': K := KRight;
        'I': K := KPgUp;  'Q': K := KPgDn;
        'G': K := KHome;  'O': K := KEnd;
        'S': K := KDel;
      end;
    end;
  end
  else if C = #27 then
  begin
    if WaitKey(300) then
    begin
      Read(Kbd, C);
      if C = '[' then
      begin
        K := 0;
        if WaitKey(2000) then
        begin
          Read(Kbd, C);
          case C of
            'A': K := KUp;   'B': K := KDown;
            'C': K := KRight; 'D': K := KLeft;
          end;
        end;
      end;
    end;
  end
  else
    case K of
      11, 5:  K := KUp;       { ^K ^E }
      10, 24: K := KDown;     { ^J ^X }
      8, 19:  K := KLeft;     { ^H ^S }
      12, 4:  K := KRight;    { ^L ^D }
      18:     K := KPgUp;     { ^R }
      3:      K := KPgDn;     { ^C }
      127:    K := KDel;
    end;
  GetKey := K;
end;

function WaitAnyKey: integer;
var K: integer;
begin
  repeat
    Service;
    K := GetKey;
  until K <> 0;
  WaitAnyKey := K;
end;
