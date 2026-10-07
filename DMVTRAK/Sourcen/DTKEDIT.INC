{ DTKEDIT.INC - Bildschirmaufbau, Editor, Menue, Instrumente, Reihenfolge }

const
  PianoLo: string[13] = 'YSXDCVGBHNJM,';           { C .. C }
  PianoHi: string[17] = 'Q2W3ER5T6Z7UI9O0P';       { C .. E }
  RX       = 58;                                     { rechte Spalte }

var
  EdO, EdR, EdC, EdF : integer;
  Oct, CurIns        : integer;
  ChOfs, PageTop     : integer;
  Msg                : Str80;
  LineDirty          : array[0..23] of boolean;
  Quit               : boolean;
  LastCX, LastCY     : integer;
  ShownMark          : integer;   { Zeile mit Abspielmarke, -1 keine }
  WantChanged        : boolean;   { seit dem letzten Show eine Zeile neu }

procedure DirtyAll;
var I: integer;
begin
  for I := 0 to ScrRows - 1 do LineDirty[I] := true;
end;

function CellX(K: integer): integer;     { K = sichtbarer Kanal 0..3 }
begin
  CellX := 4 + K * 13;
end;

function VisCh: integer;
begin
  if NChan < 4 then VisCh := NChan else VisCh := 4;
end;

function CurPat: integer;
begin
  CurPat := Order[EdO];
end;

function SignStr(N: integer): Str20;
begin
  if N >= 0 then SignStr := '+' + IStr(N) else SignStr := IStr(N);
end;

function YesNo(B: boolean): Str20;
begin
  if B then YesNo := 'J' else YesNo := 'N';
end;

procedure BuildPatLine(Y: integer);
var R, K, C: integer;
    S: Str80;
begin
  R := PageTop + Y - Top;
  S := Dec2(R);
  if (ShownMark = R) then S := S + '>' else S := S + ' ';
  S := S + ' ';
  for K := 0 to VisCh - 1 do
  begin
    C := ChOfs + K;
    S := S + '| ' + CellText(CurPat, R, C) + ' ';
    PitTrack;
  end;
  S := S + '|';
  Want[Y] := Pad(S, RX - 1);
end;

procedure BuildSide(Y: integer);      { rechte Spalte, Zeile Y }
var S: Str80;
    I: integer;
begin
  S := '';
  case Y of
    3:  S := 'Position ' + Dec2(EdO) + ' von ' + Dec2(SongLen);
    4:  S := 'Pattern  ' + Dec2(CurPat);
    5:  S := 'Zeile    ' + Dec2(EdR);
    6:  S := 'Speed ' + IStr(InitSpd) + '  BPM ' + IStr(InitBPM);
    7:  if UseSyn then S := 'Klang    Synthesizer'
        else if ArpMode then S := 'Klang 1 St. Arpeggio'
        else S := 'Klang 1 St. fuehrend';
    8:  S := 'Wiederh. ' + YesNo(Looping) + ' ab ' + Dec2(Restart);
    9:  S := 'Transp.  ' + SignStr(GTrans);
    10: S := 'Oktave   ' + IStr(Oct);
    12: S := 'Instrument ' + Hex(CurIns, 2);
    13: S := IName[CurIns];
    14: S := 'Prio ' + IStr(IPrio[CurIns]) + '  Tr ' + SignStr(ITrans[CurIns]) +
             '  Vol ' + IStr(IVol[CurIns]);
    15: S := 'Halten ' + YesNo(IHold[CurIns]) + '  Rauschen ' + YesNo(INoise[CurIns]);
    17: if Playing then S := '*** spielt ***' else S := '    Stopp';
    18: begin
          S := '';
          for I := EdO - 2 to EdO + 3 do
            if (I >= 0) and (I < SongLen) then
            begin
              if I = EdO then S := S + '[' + Dec2(Order[I]) + ']'
              else S := S + ' ' + Dec2(Order[I]) + ' ';
            end;
        end;
  end;
  PitTrack;
  Put(RX, Y, Pad(S, 79 - RX));
end;

procedure BuildLine(Y: integer);
var S: Str80;
    K: integer;
begin
  WantChanged := true;
  case Y of
    0: begin
         S := 'DMVTRAK ' + Version + ' - Tracker fuer den DMV-Lautsprecher';
         if Modified then S := Pad(S, 66) + '(geaendert)';
         Want[0] := S;
       end;
    1: Want[1] := 'Lied: ' + Pad(Title, 22) + '  Datei: ' + FileName;
    2: begin
         S := 'Zl  ';
         for K := 0 to VisCh - 1 do
           S := S + '| Kanal ' + IStr(ChOfs + K + 1) + '    ';
         S := S + '|';
         if NChan > 4 then S := S + ' (' + IStr(NChan) + ' Kanaele)';
         Want[2] := Pad(S, RX - 1);
       end;
    20: Want[20] := 'Pfeile Zelle  TAB Kanal  < > Position  + - Oktave  Leertaste Spielen/Stopp';
    21: Want[21] := 'Noten Y S X D C V G B H N J M , / Q 2 W 3 E R 5 T 6 Z 7 U I  . loeschen 1 aus';
    22: Want[22] := 'Hex-Felder 0-9 A-F   RETURN Lied von vorn   ESC Menue (Laden, Speichern ...)';
    23: Want[23] := Msg;
  else
    Want[Y] := '';
  end;
  PitTrack;
  if (Y >= Top) and (Y < Top + PageRows) then BuildPatLine(Y);
  PitTrack;
  if (Y >= 3) and (Y <= 18) then BuildSide(Y);
  PitTrack;
end;

procedure SetCursor;
const FOfs: array[0..5] of integer = (0, 4, 5, 7, 8, 9);
begin
  CurY := Top + EdR - PageTop;
  CurX := CellX(EdC - ChOfs) + 2 + FOfs[EdF];
end;

procedure FixView;           { Seite und Kanalfenster dem Cursor nachfuehren }
var Old: integer;
begin
  Old := PageTop;
  if EdR < PageTop then PageTop := EdR;
  if EdR >= PageTop + PageRows then PageTop := EdR - PageRows + 1;
  if PageTop <> Old then DirtyAll;
  Old := ChOfs;
  if EdC < ChOfs then ChOfs := EdC;
  if EdC >= ChOfs + VisCh then ChOfs := EdC - VisCh + 1;
  if ChOfs <> Old then DirtyAll;
end;

procedure Refresh;           { alle markierten Zeilen neu aufbauen }
var Y: integer;
begin
  for Y := 0 to ScrRows - 1 do
    if LineDirty[Y] then
    begin
      BuildLine(Y);
      LineDirty[Y] := false;
      Service;
    end;
end;

function RefreshOne: boolean;    { eine Zeile aufbauen; false = nichts zu tun }
var Y: integer;
begin
  RefreshOne := false;
  for Y := 0 to ScrRows - 1 do
    if LineDirty[Y] then
    begin
      BuildLine(Y);
      LineDirty[Y] := false;
      RefreshOne := true;
      exit;
    end;
end;

procedure Show;
begin
  SetCursor;
  if WantChanged or (CurX <> LastCX) or (CurY <> LastCY) then
  begin
    WantChanged := false;
    Flush;
    LastCX := CurX; LastCY := CurY;
  end;
end;

procedure SetMsg(S: Str80);
begin
  Msg := S;
  LineDirty[23] := true;
end;

procedure SideDirty;
var Y: integer;
begin
  for Y := 0 to 18 do LineDirty[Y] := true;
end;

{ ---------------- Eingabezeile ---------------- }

function InputStr(Prompt, Def: Str80; var Ok: boolean): Str80;
var S: Str80;
    K: integer;
begin
  S := Def;
  Ok := false;
  repeat
    Want[23] := Prompt + S;
    CurX := Length(Want[23]); CurY := 23;
    if CurX > 78 then CurX := 78;
    Flush;
    LastCX := -1;
    K := WaitAnyKey;
    if (K = KLeft) or (K = KDel) then
    begin
      if Length(S) > 0 then S := Copy(S, 1, Length(S) - 1);
    end
    else if (K >= 32) and (K < 127) and (Length(Prompt) + Length(S) < 77) then
      S := S + Chr(K);
  until (K = KRet) or (K = KEsc);
  Ok := K = KRet;
  InputStr := S;
  SetMsg('');
end;

function InputNum(Prompt: Str80; Def, Lo, Hi: integer; var V: integer): boolean;
var S: Str80;
    Ok: boolean;
    N, E: integer;
begin
  InputNum := false;
  S := InputStr(Prompt, IStr(Def), Ok);
  if not Ok then exit;
  Val(S, N, E);
  if (E <> 0) or (N < Lo) or (N > Hi) then
  begin
    SetMsg('Ungueltig, erlaubt ' + IStr(Lo) + ' bis ' + IStr(Hi));
    exit;
  end;
  V := N;
  InputNum := true;
end;

function Confirm(Prompt: Str80): boolean;
var K: integer;
begin
  Want[23] := Prompt + ' (J/N) ';
  CurX := Length(Want[23]); CurY := 23;
  Flush;
  LastCX := -1;
  K := WaitAnyKey;
  Confirm := (K = Ord('j')) or (K = Ord('J'));
  SetMsg('');
end;

{ ---------------- Instrumente ---------------- }

procedure InsScreen;
var Sel, I, K, X, Y, V: integer;
    S: Str80;
    Ok, Done: boolean;
begin
  Sel := CurIns;
  Done := false;
  ScrBlank;
  repeat
    Want[0] := 'Instrumente  (Prio 0 = stumm, hoehere gewinnt; J = Ton haelt; R = Rauschen)';
    for I := 1 to NIns do
    begin
      if I <= 16 then begin X := 0; Y := I + 1; end
      else begin X := 40; Y := I - 15; end;
      if I = Sel then S := '>' else S := ' ';
      S := S + Hex(I, 2) + ' ' + Pad(IName[I], 17) + ' ' + IStr(IPrio[I]) + ' ' +
           Pad(SignStr(ITrans[I]), 3) + ' ' + YesNo(IHold[I]);
      if INoise[I] then S := S + ' R' else S := S + '  ';
      Put(X, Y, Pad(S, 39));
    end;
    Want[18] := 'Nr ' + Hex(Sel, 2) + ': ' + Pad(IName[Sel], 22) + '  Vol ' + IStr(IVol[Sel]) +
                '  Sample ' + IStr(Round(ILen[Sel] / 2)) + ' Worte';
    Want[20] := 'Pfeile waehlen  0-9 Prioritaet  + - Halbton  < > Oktave  H Halten  V Lautst.';
    Want[21] := 'R Rauschen  N Name  P Probe  RETURN als aktuelles Instrument  ESC zurueck';
    Want[23] := Msg;
    if Sel <= 16 then begin CurX := 0; CurY := Sel + 1; end
    else begin CurX := 40; CurY := Sel - 15; end;
    Flush;
    K := WaitAnyKey;
    SetMsg('');
    if K = KUp then begin if Sel > 1 then Sel := Sel - 1; end
    else if K = KDown then begin if Sel < NIns then Sel := Sel + 1; end
    else if K = KLeft then begin if Sel > 16 then Sel := Sel - 16; end
    else if K = KRight then begin if Sel + 16 <= NIns then Sel := Sel + 16; end
    else if (K >= Ord('0')) and (K <= Ord('9')) then
      begin IPrio[Sel] := K - Ord('0'); Modified := true; end
    else if K = Ord('+') then
      begin if ITrans[Sel] < 36 then ITrans[Sel] := ITrans[Sel] + 1; Modified := true; end
    else if K = Ord('-') then
      begin if ITrans[Sel] > -36 then ITrans[Sel] := ITrans[Sel] - 1; Modified := true; end
    else if K = Ord('>') then
      begin if ITrans[Sel] <= 24 then ITrans[Sel] := ITrans[Sel] + 12; Modified := true; end
    else if K = Ord('<') then
      begin if ITrans[Sel] >= -24 then ITrans[Sel] := ITrans[Sel] - 12; Modified := true; end
    else if UpCase(Chr(K and 127)) = 'H' then
      begin IHold[Sel] := not IHold[Sel]; Modified := true; end
    else if UpCase(Chr(K and 127)) = 'R' then
      begin INoise[Sel] := not INoise[Sel]; Modified := true; end
    else if UpCase(Chr(K and 127)) = 'V' then
      begin
        if InputNum('Lautstaerke (0-64, 0 = stumm): ', IVol[Sel], 0, 64, V) then
        begin IVol[Sel] := V; Modified := true; end;
      end
    else if UpCase(Chr(K and 127)) = 'N' then
      begin
        S := InputStr('Name: ', IName[Sel], Ok);
        if Ok then begin IName[Sel] := Copy(S, 1, 22); Modified := true; end;
      end
    else if UpCase(Chr(K and 127)) = 'P' then
      begin
        SndClear;
        Tone(ToneOf(25, Sel), 6); Tone(ToneOf(29, Sel), 6);
        Tone(ToneOf(32, Sel), 6); Tone(ToneOf(37, Sel), 12);
      end
    else if K = KRet then
      begin CurIns := Sel; Done := true; end
    else if K = KEsc then Done := true;
  until Done;
  ScrBlank;
  DirtyAll;
end;

{ ---------------- Reihenfolge ---------------- }

procedure OrderScreen;
var Sel, I, K, D, V: integer;
    S: Str80;
    Done: boolean;
    Digit: integer;     { erste Ziffer einer Zweiereingabe, -1 keine }
begin
  Sel := EdO;
  Done := false; Digit := -1;
  ScrBlank;
  repeat
    Want[0] := 'Reihenfolge  (Laenge ' + IStr(SongLen) + ', Wiederholpunkt ' + Dec2(Restart) + ')';
    for I := 0 to 127 do
    begin
      if I < SongLen then S := Dec2(Order[I]) else S := '..';
      if I = Restart then S := S + '*' else S := S + ' ';
      if I = Sel then S := '[' + S else S := ' ' + S;
      Put((I mod 16) * 4 + 4, I div 16 + 2, S);
    end;
    for I := 0 to 7 do Put(0, I + 2, Dec2(I * 16));
    Want[12] := 'Position ' + Dec2(Sel) + ': Pattern ' + Dec2(Order[Sel]);
    Want[20] := 'Pfeile waehlen  Ziffern Patternnummer  E einfuegen  L loeschen  W Wiederholpunkt';
    Want[21] := 'F freies neues Pattern  RETURN dorthin springen  ESC zurueck';
    Want[23] := Msg;
    CurX := (Sel mod 16) * 4 + 5; CurY := Sel div 16 + 2;
    Flush;
    K := WaitAnyKey;
    SetMsg('');
    if (K >= Ord('0')) and (K <= Ord('9')) then
    begin
      D := K - Ord('0');
      if Digit < 0 then
      begin
        Digit := D;
        SetMsg('Patternnummer: ' + IStr(D) + '_');
      end
      else
      begin
        V := Digit * 10 + D;
        Digit := -1;
        if V < MaxPat then
        begin
          if AllocPat(V) then
          begin
            Order[Sel] := V;
            if Sel >= SongLen then SongLen := Sel + 1;
            Modified := true;
          end
          else SetMsg(LastErr);
        end;
      end;
    end
    else
    begin
      Digit := -1;
      if K = KLeft then begin if Sel > 0 then Sel := Sel - 1; end
      else if K = KRight then begin if Sel < 127 then Sel := Sel + 1; end
      else if K = KUp then begin if Sel >= 16 then Sel := Sel - 16; end
      else if K = KDown then begin if Sel + 16 <= 127 then Sel := Sel + 16; end
      else if UpCase(Chr(K and 127)) = 'E' then
      begin
        if SongLen < 128 then
        begin
          for I := 127 downto Sel + 1 do Order[I] := Order[I - 1];
          SongLen := SongLen + 1;
          if Restart > Sel then Restart := Restart + 1;
          Modified := true;
        end;
      end
      else if (UpCase(Chr(K and 127)) = 'L') or (K = KDel) then
      begin
        if (SongLen > 1) and (Sel < SongLen) then
        begin
          for I := Sel to 126 do Order[I] := Order[I + 1];
          Order[127] := 0;
          SongLen := SongLen - 1;
          if Restart >= SongLen then Restart := 0;
          if Sel >= SongLen then Sel := SongLen - 1;
          Modified := true;
        end;
      end
      else if UpCase(Chr(K and 127)) = 'W' then
      begin
        if Sel < SongLen then begin Restart := Sel; Modified := true; end;
      end
      else if UpCase(Chr(K and 127)) = 'F' then
      begin
        V := 0;
        while (V < MaxPat) and (Pat[V] <> nil) do V := V + 1;
        if V >= MaxPat then SetMsg('Kein freies Pattern mehr')
        else if AllocPat(V) then
        begin
          Order[Sel] := V;
          if Sel >= SongLen then SongLen := Sel + 1;
          Modified := true;
          SetMsg('Neues Pattern ' + Dec2(V));
        end
        else SetMsg(LastErr);
      end
      else if K = KRet then
      begin
        if Sel < SongLen then EdO := Sel;
        Done := true;
      end
      else if K = KEsc then Done := true;
    end;
  until Done;
  if EdO >= SongLen then EdO := SongLen - 1;
  ScrBlank;
  DirtyAll;
end;

{ ---------------- Menue ---------------- }

procedure DoLoad;
var S: Str80;
    Ok: boolean;
begin
  if Modified then
    if not Confirm('Aenderungen gehen verloren. Trotzdem laden?') then exit;
  S := InputStr('Laden (MOD oder DTK): ', '', Ok);
  if (not Ok) or (S = '') then exit;
  SetMsg('Lade ' + S + ' ...');
  Refresh; Show;
  if LoadAny(S) then
    SetMsg('Geladen: ' + FileName + ', ' + IStr(NPat) + ' Patterns, ' +
           IStr(NChan) + ' Kanaele')
  else
    SetMsg(LastErr);
  EdO := 0; EdR := 0; EdC := 0; EdF := 0; ChOfs := 0; PageTop := 0;
  CurIns := 1;
  DirtyAll;
end;

procedure DoSave;
var S: Str80;
    Ok: boolean;
begin
  S := FileName;
  if S = '' then S := 'LIED.DTK';
  S := InputStr('Speichern als: ', S, Ok);
  if (not Ok) or (S = '') then exit;
  if SaveDtk(S) then SetMsg('Gespeichert: ' + FileName)
  else SetMsg(LastErr);
  DirtyAll;
end;

procedure Menu;
var K, V: integer;
    S: Str80;
    Ok: boolean;
begin
  Want[23] := 'L)aden S)peichern N)eu I)nstr. R)eihenf. M)odus/Klang T)empo W)iederh. U)eberschr. E)nde';
  Want[23] := Copy(Want[23], 1, 79);
  CurX := 78; CurY := 23;
  Flush;
  LastCX := -1;
  K := WaitAnyKey;
  SetMsg('');
  case UpCase(Chr(K and 127)) of
    'L': DoLoad;
    'S': DoSave;
    'N': if Confirm('Neues Lied, alles loeschen?') then
         begin
           NewSong;
           EdO := 0; EdR := 0; EdC := 0; EdF := 0; ChOfs := 0; PageTop := 0;
           DirtyAll;
         end;
    'I': InsScreen;
    'R': OrderScreen;
    'M': begin
           { Synthesizer -> 1 Stimme fuehrend -> 1 Stimme Arpeggio -> ... }
           if UseSyn then begin UseSyn := false; ArpMode := false; end
           else if not ArpMode then ArpMode := true
           else if SynAvail and PitOk then begin UseSyn := true; ArpMode := false; end
           else ArpMode := false;
           if not UseSyn then Modified := true;
           SideDirty;
           if UseSyn then SetMsg('Klang: Synthesizer (3 Stimmen + Rauschen)')
           else if SynAvail and PitOk then SetMsg('Klang: eine Stimme ueber Befehl 06h')
           else SetMsg('Keine Synthesizer-Firmware im 8741 - nur eine Stimme');
         end;
    'T': begin
           if InputNum('Speed (Ticks je Zeile, 1-31): ', InitSpd, 1, 31, V) then
           begin
             InitSpd := V;
             if InputNum('BPM (32-255, 125 = 20 ms je Tick): ', InitBPM, 32, 255, V) then
               InitBPM := V;
             Modified := true;
           end;
           SideDirty;
         end;
    'W': begin Looping := not Looping; Modified := true; SideDirty; end;
    'U': begin
           S := InputStr('Titel: ', Title, Ok);
           if Ok then begin Title := Copy(S, 1, 20); Modified := true; end;
           DirtyAll;
         end;
    'G': begin
           if InputNum('Transponierung gesamt (-24..24): ', GTrans, -24, 24, V) then
           begin GTrans := V; Modified := true; end;
           SideDirty;
         end;
    'E': if not Modified then Quit := true
         else if Confirm('Nicht gespeichert. Trotzdem beenden?') then Quit := true;
  end;
end;

{ ---------------- Editieren ---------------- }

procedure MoveDown;
begin
  if EdR < Rows - 1 then EdR := EdR + 1;
end;

procedure EnterNote(Semi: integer);
var N: integer;
begin
  N := Oct * 12 + Semi + 1;
  if (N < 1) or (N > 60) then exit;
  SetCell(CurPat, EdR, EdC, 0, N);
  SetCell(CurPat, EdR, EdC, 1, CurIns);
  SndClear;
  Tone(ToneOf(N, CurIns), 8);
  LineDirty[Top + EdR - PageTop] := true;
  MoveDown;
end;

procedure EnterHex(D: integer);
var V: integer;
begin
  case EdF of
    1: begin
         V := D * 16 + (CellB(CurPat, EdR, EdC, 1) and 15);
         if V > NIns then V := NIns;
         SetCell(CurPat, EdR, EdC, 1, V);
         if V > 0 then CurIns := V;
       end;
    2: begin
         V := (CellB(CurPat, EdR, EdC, 1) and $F0) + D;
         if V > NIns then V := NIns;
         SetCell(CurPat, EdR, EdC, 1, V);
         if V > 0 then CurIns := V;
       end;
    3: SetCell(CurPat, EdR, EdC, 2, D);
    4: SetCell(CurPat, EdR, EdC, 3, D * 16 + (CellB(CurPat, EdR, EdC, 3) and 15));
    5: SetCell(CurPat, EdR, EdC, 3, (CellB(CurPat, EdR, EdC, 3) and $F0) + D);
  end;
  LineDirty[Top + EdR - PageTop] := true;
  MoveDown;
end;

procedure ClearField;
begin
  case EdF of
    0: begin SetCell(CurPat, EdR, EdC, 0, 0); SetCell(CurPat, EdR, EdC, 1, 0); end;
    1, 2: SetCell(CurPat, EdR, EdC, 1, 0);
    3, 4, 5: begin SetCell(CurPat, EdR, EdC, 2, 0); SetCell(CurPat, EdR, EdC, 3, 0); end;
  end;
  LineDirty[Top + EdR - PageTop] := true;
  MoveDown;
end;

function HexVal(K: integer): integer;
var C: char;
begin
  HexVal := -1;
  if (K < 32) or (K > 126) then exit;
  C := UpCase(Chr(K));
  if (C >= '0') and (C <= '9') then HexVal := Ord(C) - Ord('0')
  else if (C >= 'A') and (C <= 'F') then HexVal := Ord(C) - Ord('A') + 10;
end;

procedure EditKey(K: integer);
var OldO, I: integer;
    C: char;
begin
  OldO := EdO;
  if K = KUp then begin if EdR > 0 then EdR := EdR - 1 else EdR := Rows - 1; end
  else if K = KDown then begin if EdR < Rows - 1 then EdR := EdR + 1 else EdR := 0; end
  else if K = KPgUp then begin EdR := EdR - PageRows; if EdR < 0 then EdR := 0; end
  else if K = KPgDn then begin EdR := EdR + PageRows; if EdR > Rows - 1 then EdR := Rows - 1; end
  else if K = KHome then EdR := 0
  else if K = KEnd then EdR := Rows - 1
  else if K = KLeft then
  begin
    if EdF > 0 then EdF := EdF - 1
    else if EdC > 0 then begin EdC := EdC - 1; EdF := 5; end;
  end
  else if K = KRight then
  begin
    if EdF < 5 then EdF := EdF + 1
    else if EdC < NChan - 1 then begin EdC := EdC + 1; EdF := 0; end;
  end
  else if K = KTab then
  begin
    EdF := 0;
    if EdC < NChan - 1 then EdC := EdC + 1 else EdC := 0;
  end
  else if K = Ord('<') then begin if EdO > 0 then EdO := EdO - 1; end
  else if K = Ord('>') then begin if EdO < SongLen - 1 then EdO := EdO + 1; end
  else if K = Ord('+') then begin if Oct < 3 then Oct := Oct + 1; SideDirty; end
  else if K = Ord('-') then begin if Oct > 0 then Oct := Oct - 1; SideDirty; end
  else if K = Ord(' ') then
  begin
    PlayStart(EdO, EdR, false);
    ShownMark := -1;
    SideDirty;
  end
  else if K = KRet then
  begin
    PlayStart(0, 0, false);
    ShownMark := -1;
    SideDirty;
  end
  else if (K = Ord('.')) or (K = KDel) then ClearField
  else if K = KEsc then Menu
  else if (EdF = 0) and (K = Ord('1')) then
  begin
    SetCell(CurPat, EdR, EdC, 0, 0); SetCell(CurPat, EdR, EdC, 1, 0);
    SetCell(CurPat, EdR, EdC, 2, $C); SetCell(CurPat, EdR, EdC, 3, 0);
    LineDirty[Top + EdR - PageTop] := true;
    MoveDown;
  end
  else if (EdF = 0) and (K > 32) and (K < 127) then
  begin
    C := UpCase(Chr(K));
    I := Pos(C, PianoLo);
    if I > 0 then EnterNote(I - 1)
    else
    begin
      I := Pos(C, PianoHi);
      if I > 0 then EnterNote(I + 11);
    end;
  end
  else if (EdF > 0) and (HexVal(K) >= 0) then EnterHex(HexVal(K));
  if EdO <> OldO then DirtyAll;
  if Modified then LineDirty[0] := true;
  SideDirty;
  FixView;
end;

var EditActive: boolean;        { Editor zeigt das Pattern (fuer DisplayStale) }

function DisplayStale;          { forward in DTKSCR }
begin
  DisplayStale := EditActive and Playing and
                  ((NowO <> EdO) or (NowR < PageTop) or (NowR >= PageTop + PageRows));
end;

procedure FollowPlay;          { Anzeige folgt dem Abspielen }
var Old: integer;
begin
  Old := ShownMark;
  if NowO <> EdO then
  begin
    EdO := NowO;
    DirtyAll;
  end;
  EdR := NowR;
  if (EdR < PageTop) or (EdR >= PageTop + PageRows) then
  begin
    PageTop := (EdR div PageRows) * PageRows;
    DirtyAll;
  end;
  ShownMark := EdR;
  if (Old >= PageTop) and (Old < PageTop + PageRows) then
    LineDirty[Top + Old - PageTop] := true;
  LineDirty[Top + EdR - PageTop] := true;
  LineDirty[3] := true; LineDirty[4] := true; LineDirty[5] := true;
  LineDirty[18] := true;
end;

procedure Editor;
var K: integer;
    WasPlaying: boolean;
begin
  EdO := 0; EdR := 0; EdC := 0; EdF := 0;
  Oct := 1; CurIns := 1; ChOfs := 0; PageTop := 0;
  Msg := 'DMVTRAK ' + Version + ' - Code: Claude, Prompts: rfka01 2026';
  WantChanged := true;
  Quit := false; ShownMark := -1;
  Playing := false; PlEnd := true;
  EditActive := true;
  ScrReset; ScrBlank;
  LastCX := -1; LastCY := -1;
  DirtyAll;
  Refresh;
  Show;
  repeat
    WasPlaying := Playing;
    PlayService;
    if Playing then
    begin
      if NowNew then begin NowNew := false; FollowPlay; end;
      if not RefreshOne then Show;
    end
    else
    begin
      if WasPlaying then
      begin
        ShownMark := -1;
        DirtyAll;
      end;
      Refresh;
      Show;
    end;
    K := GetKey;
    if K <> 0 then
    begin
      if Playing then
      begin
        if (K = Ord(' ')) or (K = KEsc) or (K = KRet) then
        begin
          PlayStop;
          ShownMark := -1;
          FixView;
          DirtyAll;
        end;
      end
      else
        EditKey(K);
    end;
  until Quit;
  EditActive := false;
  PlayStop;
  ConOut(#26);
end;

{ ---------------- reiner Abspielbetrieb (DMVTRAK datei /P) ---------------- }

procedure PlayerMode;
var K, C: integer;
    S: Str80;
begin
  EditActive := false;
  ScrReset; ScrBlank;
  Want[0] := 'DMVTRAK ' + Version + ' spielt ' + FileName;
  Want[1] := 'Lied: ' + Title;
  if UseSyn then S := 'Synthesizer (3 Stimmen + Rauschen)'
  else if ArpMode then S := '1 Stimme, Arpeggio' else S := '1 Stimme, fuehrend';
  Want[2] := IStr(NChan) + ' Kanaele, ' + IStr(SongLen) + ' Positionen, ' + S;
  Want[6] := 'Leertaste oder ESC: Ende';
  CurX := 0; CurY := 7;
  Flush;
  PlayStart(0, 0, true);
  K := 0;
  while Playing and (K = 0) do
  begin
    PlayService;
    if NowNew then
    begin
      NowNew := false;
      S := 'Position ' + Dec2(NowO) + '/' + Dec2(SongLen);
      PitTrack;
      S := S + '  Pattern ' + Dec2(Order[NowO]);
      PitTrack;
      S := S + '  Zeile ' + Dec2(NowR) + '   ';
      PitTrack;
      for C := 0 to VisCh - 1 do
      begin
        S := S + CellText(Order[NowO], NowR, C) + ' ';
        PitTrack;
      end;
      Want[4] := S;
      Flush;
    end;
    K := GetKey;
  end;
  PlayStop;
  ConOut(#26);
end;
