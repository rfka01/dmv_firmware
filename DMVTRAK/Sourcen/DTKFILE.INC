{ DTKFILE.INC - Laden (MOD und DTK) und Speichern (DTK)

  Turbo Pascal 3 liest untypisierte Dateien in 128-Byte-Saetzen;
  RByte/WByte puffern darum selbst. Vom MOD werden Kopf und Patterns
  gelesen, die Samples uebersprungen (nur Laenge und Schleife zaehlen).

  DTK-Format (DMVTRAK1), alle Zahlen Intel-Reihenfolge:
     0  'DMVTRAK1'
     8  Titel (20 Byte)
    28  Kanaele, Liedlaenge, Wiederholpunkt, Patterns, Schalter
        (Bit 0 Arpeggio, Bit 1 Wiederholen), Transp.+128, Speed, BPM
   128  31 Instrumente je 32 Byte: Name 22, Prioritaet, Transp.+128,
        Lautstaerke, Halten, Samplelaenge 4 Byte, Rauschen, 1 frei
  1120  Reihenfolge (128 Byte)
  1248  Patterns: je 64 Zeilen x Kanaele x 4 Byte (Note, Instr., Eff., Par.)
  Die Datei ist auf volle 128 Byte aufgefuellt. }

const
  HdrLen = 1248;            { DTK-Kopf; beim MOD schon Patterndaten }

var
  RF       : file;
  RB, WB   : array[0..127] of byte;
  RPos     : integer;
  WPos     : integer;
  RErr     : boolean;
  HB       : array[0..1247] of byte;
  HPos     : integer;

function RByte: integer;
begin
  if RPos >= 128 then
  begin
    FillChar(RB, 128, 0);
    {$I-} BlockRead(RF, RB, 1); {$I+}
    if IOresult <> 0 then RErr := true;
    RPos := 0;
  end;
  RByte := RB[RPos];
  RPos := RPos + 1;
end;

function PByte: integer;       { erst Kopfpuffer, dann Datei }
begin
  if HPos < HdrLen then
  begin
    PByte := HB[HPos];
    HPos := HPos + 1;
  end
  else
    PByte := RByte;
end;

procedure WByte(B: integer);
begin
  WB[WPos] := B and 255;
  WPos := WPos + 1;
  if WPos >= 128 then
  begin
    {$I-} BlockWrite(RF, WB, 1); {$I+}
    if IOresult <> 0 then RErr := true;
    WPos := 0;
  end;
end;

function Upper(S: Str80): Str80;
var I: integer;
begin
  for I := 1 to Length(S) do S[I] := UpCase(S[I]);
  Upper := S;
end;

function HasExt(S: Str80): boolean;
var I: integer;
begin
  HasExt := false;
  for I := Length(S) downto 1 do
  begin
    if S[I] = '.' then begin HasExt := true; exit; end;
    if (S[I] = '\') or (S[I] = ':') then exit;
  end;
end;

function ChgExt(S: Str80; E: Str20): Str80;   { E mit Punkt }
var I: integer;
begin
  if HasExt(S) then
  begin
    I := Length(S);
    while S[I] <> '.' do I := I - 1;
    S := Copy(S, 1, I - 1);
  end;
  ChgExt := S + E;
end;

function OpenRead(N: Str80): boolean;
begin
  Assign(RF, N);
  {$I-} Reset(RF); {$I+}
  OpenRead := IOresult = 0;
end;

function Printable(B: integer): char;
begin
  if (B >= 32) and (B < 127) then Printable := Chr(B) else Printable := ' ';
end;

function TrimR(S: Str80): Str80;
begin
  while (Length(S) > 0) and (S[Length(S)] = ' ') do S := Copy(S, 1, Length(S) - 1);
  TrimR := S;
end;

function ReadPatterns(Fmt: integer): boolean;   { 0 = MOD, 1 = DTK }
var P, R, C, B0, B1, B2, B3, Per, Smp, Base: integer;
begin
  ReadPatterns := false;
  for P := 0 to NPat - 1 do
  begin
    if not AllocPat(P) then exit;
    for R := 0 to Rows - 1 do
      for C := 0 to NChan - 1 do
      begin
        B0 := PByte; B1 := PByte; B2 := PByte; B3 := PByte;
        Base := (R * NChan + C) * 4;
        if Fmt = 0 then
        begin
          Per := (B0 and $0F) shl 8 + B1;
          Smp := (B0 and $F0) + B2 shr 4;
          if Per = 0 then Pat[P]^[Base] := 0
          else Pat[P]^[Base] := NoteOfPer(Per);
          Pat[P]^[Base + 1] := Smp and 31;
          Pat[P]^[Base + 2] := B2 and $0F;
          Pat[P]^[Base + 3] := B3;
        end
        else
        begin
          if B0 > 60 then B0 := 0;
          Pat[P]^[Base] := B0;
          Pat[P]^[Base + 1] := B1 and 31;
          Pat[P]^[Base + 2] := B2 and $0F;
          Pat[P]^[Base + 3] := B3;
        end;
      end;
    if RErr then
    begin
      LastErr := 'Datei zu kurz (Pattern ' + IStr(P) + ')';
      exit;
    end;
  end;
  ReadPatterns := true;
end;

{ Prioritaeten nach Gebrauch: Instrumente mit hoechstens 2 verschiedenen
  Noten gelten als Schlagzeug (1). Die uebrigen werden nach ihrer
  mittleren Tonhoehe gereiht, die hoechste Stimme bekommt 9, die naechste
  8 usw. (mindestens 3) - meist ist die hoechste Stimme die Melodie. }
procedure AutoPrio;
var NS   : array[1..31] of set of 0..63;
    Sum  : array[1..31] of real;
    Cnt  : array[1..31] of integer;
    Mel  : array[1..31] of boolean;
    P, R, C, N, I, D, Best, Rank: integer;
    BestA: real;
begin
  for I := 1 to NIns do begin NS[I] := []; Sum[I] := 0; Cnt[I] := 0; end;
  for P := 0 to NPat - 1 do
    if Pat[P] <> nil then
      for R := 0 to Rows - 1 do
        for C := 0 to NChan - 1 do
        begin
          N := CellB(P, R, C, 0); I := CellB(P, R, C, 1);
          if (N > 0) and (I >= 1) and (I <= NIns) then
          begin
            NS[I] := NS[I] + [N];
            Sum[I] := Sum[I] + N;
            Cnt[I] := Cnt[I] + 1;
          end;
        end;
  for I := 1 to NIns do
  begin
    D := 0;
    for N := 1 to 60 do if N in NS[I] then D := D + 1;
    Mel[I] := (D >= 3) and (IPrio[I] > 0);
    if (IPrio[I] > 0) and (Cnt[I] > 0) and not Mel[I] then
    begin
      IPrio[I] := 1;
      INoise[I] := true;
    end;
  end;
  Rank := 9;
  repeat
    Best := 0; BestA := -1;
    for I := 1 to NIns do
      if Mel[I] then
        if Sum[I] / Cnt[I] > BestA then
        begin
          Best := I; BestA := Sum[I] / Cnt[I];
        end;
    if Best > 0 then
    begin
      IPrio[Best] := Rank;
      Mel[Best] := false;
      if Rank > 3 then Rank := Rank - 1;
    end;
  until Best = 0;
end;

function LoadMod: boolean;      { Kopf steht in HB }
var Tag: string[4];
    I, J, NSmp, B, Rl: integer;
    S: string[22];
begin
  LoadMod := false;
  Tag := '';
  for I := 1080 to 1083 do Tag := Tag + Chr(HB[I]);
  NSmp := 31; I := 4;
  if (Tag = 'M.K.') or (Tag = 'M!K!') or (Tag = 'FLT4') or (Tag = '4CHN') then I := 4
  else if Tag = '6CHN' then I := 6
  else if (Tag = '8CHN') or (Tag = 'FLT8') or (Tag = 'OCTA') or (Tag = 'CD81') then I := 8
  else if (Copy(Tag, 3, 2) = 'CH') or (Copy(Tag, 2, 3) = 'CHN') then
  begin
    LastErr := 'MOD mit mehr als 8 Kanaelen (' + Tag + ')';
    exit;
  end
  else NSmp := 15;
  NChan := I;
  Title := '';
  for I := 0 to 19 do Title := Title + Printable(HB[I]);
  Title := TrimR(Title);
  for I := 1 to NIns do InsDefault(I);
  for I := 1 to NSmp do
  begin
    B := 20 + (I - 1) * 30;
    S := '';
    for J := 0 to 21 do S := S + Printable(HB[B + J]);
    IName[I] := TrimR(S);
    ILen[I] := (HB[B + 22] * 256.0 + HB[B + 23]) * 2.0;
    IVol[I] := HB[B + 25];
    if IVol[I] > 64 then IVol[I] := 64;
    Rl := HB[B + 28] * 256 + HB[B + 29];
    IHold[I] := Rl > 1;
    if ILen[I] < 4.0 then IPrio[I] := 0 else IPrio[I] := 5;
  end;
  B := 20 + NSmp * 30;
  SongLen := HB[B];
  if (SongLen < 1) or (SongLen > 128) then SongLen := 1;
  Restart := HB[B + 1];
  if Restart >= SongLen then Restart := 0;
  NPat := 0;
  for I := 0 to 127 do
  begin
    Order[I] := HB[B + 2 + I];
    if Order[I] + 1 > NPat then NPat := Order[I] + 1;
  end;
  if NPat > MaxPat then
  begin
    LastErr := 'Zu viele Patterns (' + IStr(NPat) + ')';
    exit;
  end;
  if NSmp = 31 then HPos := 1084 else HPos := 600;
  InitSpd := 6; InitBPM := 125;
  if ReadPatterns(0) then
  begin
    AutoPrio;
    LoadMod := true;
  end;
end;

function LoadDtk: boolean;
var I, J, B: integer;
    L: real;
begin
  LoadDtk := false;
  Title := '';
  for I := 8 to 27 do Title := Title + Printable(HB[I]);
  Title := TrimR(Title);
  NChan := HB[28];
  if (NChan < 1) or (NChan > MaxCh) then
  begin
    LastErr := 'DTK-Datei beschaedigt'; exit;
  end;
  SongLen := HB[29]; Restart := HB[30];
  if SongLen < 1 then SongLen := 1;
  if Restart >= SongLen then Restart := 0;
  NPat := HB[31];
  if NPat > MaxPat then begin LastErr := 'Zu viele Patterns'; exit; end;
  ArpMode := (HB[32] and 1) <> 0;
  Looping := (HB[32] and 2) <> 0;
  GTrans := HB[33] - 128;
  InitSpd := HB[34]; InitBPM := HB[35];
  if InitSpd < 1 then InitSpd := 6;
  if InitBPM < 32 then InitBPM := 125;
  for I := 1 to NIns do
  begin
    B := 128 + (I - 1) * 32;
    IName[I] := '';
    for J := 0 to 21 do IName[I] := IName[I] + Printable(HB[B + J]);
    IName[I] := TrimR(IName[I]);
    IPrio[I] := HB[B + 22];
    if IPrio[I] > 9 then IPrio[I] := 9;
    ITrans[I] := HB[B + 23] - 128;
    IVol[I] := HB[B + 24];
    if IVol[I] > 64 then IVol[I] := 64;
    IHold[I] := HB[B + 25] <> 0;
    L := 0;
    for J := 3 downto 0 do L := L * 256.0 + HB[B + 26 + J];
    ILen[I] := L;
    INoise[I] := HB[B + 30] <> 0;
  end;
  for I := 0 to 127 do Order[I] := HB[1120 + I];
  HPos := 1248;
  LoadDtk := ReadPatterns(1);
end;

function LoadAny(N: Str80): boolean;
var I: integer;
    Sig: string[8];
    Ok: boolean;
begin
  LoadAny := false;
  N := Upper(N);
  Ok := OpenRead(N);
  if (not Ok) and (not HasExt(N)) then
  begin
    Ok := OpenRead(N + '.DTK');
    if Ok then N := N + '.DTK'
    else
    begin
      Ok := OpenRead(N + '.MOD');
      if Ok then N := N + '.MOD';
    end;
  end;
  if not Ok then
  begin
    LastErr := 'Datei ' + N + ' nicht gefunden';
    exit;
  end;
  RPos := 128; RErr := false;
  for I := 0 to HdrLen - 1 do HB[I] := RByte;
  if RErr then
  begin
    Close(RF);
    LastErr := 'Datei zu kurz';
    exit;
  end;
  Sig := '';
  for I := 0 to 7 do Sig := Sig + Chr(HB[I]);
  FreeSong;
  HPos := 0;
  if Sig = 'DMVTRAK1' then Ok := LoadDtk else Ok := LoadMod;
  Close(RF);
  if not Ok then
  begin
    NewSong;
    exit;
  end;
  if Sig = 'DMVTRAK1' then FileName := N else FileName := ChgExt(N, '.DTK');
  Modified := false;
  LoadAny := true;
end;

function SaveDtk(N: Str80): boolean;
var I, J, P: integer;
    L: real;
    Sig8: string[8];
begin
  SaveDtk := false;
  N := Upper(N);
  if not HasExt(N) then N := N + '.DTK';
  Assign(RF, N);
  {$I-} Rewrite(RF); {$I+}
  if IOresult <> 0 then
  begin
    LastErr := 'Kann ' + N + ' nicht anlegen';
    exit;
  end;
  WPos := 0; RErr := false;
  { Patterns ohne Speicher als leer schreiben }
  NPat := 0;
  for I := 0 to MaxPat - 1 do if Pat[I] <> nil then NPat := I + 1;
  for I := 0 to SongLen - 1 do if Order[I] + 1 > NPat then NPat := Order[I] + 1;
  Sig8 := 'DMVTRAK1';
  for I := 1 to 8 do WByte(Ord(Sig8[I]));
  for I := 1 to 20 do
    if I <= Length(Title) then WByte(Ord(Title[I])) else WByte(0);
  WByte(NChan); WByte(SongLen); WByte(Restart); WByte(NPat);
  J := 0;
  if ArpMode then J := J + 1;
  if Looping then J := J + 2;
  WByte(J); WByte(GTrans + 128); WByte(InitSpd); WByte(InitBPM);
  for I := 36 to 127 do WByte(0);
  for I := 1 to NIns do
  begin
    for J := 1 to 22 do
      if J <= Length(IName[I]) then WByte(Ord(IName[I][J])) else WByte(0);
    WByte(IPrio[I]); WByte(ITrans[I] + 128); WByte(IVol[I]);
    if IHold[I] then WByte(1) else WByte(0);
    L := ILen[I];
    for J := 0 to 3 do
    begin
      WByte(Trunc(L - Int(L / 256.0) * 256.0));
      L := Int(L / 256.0);
    end;
    if INoise[I] then WByte(1) else WByte(0);
    WByte(0);
  end;
  for I := 0 to 127 do WByte(Order[I]);
  for P := 0 to NPat - 1 do
    for I := 0 to Rows * NChan * 4 - 1 do
      if Pat[P] = nil then WByte(0) else WByte(Pat[P]^[I]);
  while WPos <> 0 do WByte(0);
  Close(RF);
  if RErr then
  begin
    LastErr := 'Schreibfehler (Diskette voll?)';
    exit;
  end;
  FileName := N;
  Modified := false;
  SaveDtk := true;
end;
