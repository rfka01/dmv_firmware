{ DMVSYN.INC - Synthesizer und Rennwerk im Tastatur-Controller (8741) der
  NCR DMV, fuer Turbo Pascal 3. Braucht die Firmware dmv_mb_8741_32678_synth.bin
  (CRC32 bcb29bec); mit aelterer Synthesizer-Firmware (1bd9e156) geht alles
  ausser dem Rennwerk, mit der Original-Firmware nichts (SynAvail = false).

  Ablauf:   if SynAvail then
              if SynBegin then
              begin
                SynFreq(1, 440.0);  ...  SynWait(100);  ...
                SynEnd;
              end;

  Regeln:   - Nie Befehle an Port 41h schicken, ohne vorher SynAvail zu
              pruefen: Die Original-Firmware haengt bei Befehl 02h..05h.
            - Jedes Byte erst schreiben, wenn der 8741 das vorige abgeholt
              hat (Status Bit 1 = 0); das erledigt SynByte.
            - Solange der Synthesizer laeuft, fragt er die Tastatur alle
              100 ms selbst ab; KeyPressed/Read(Kbd) gehen normal, aber ohne
              Tastenklick. Jeder Befehl an Port 41h (auch vom BIOS, z. B.
              die Glocke) beendet ihn - SynRunning prueft das.
  Code: Claude (Anthropic), Idee und Prompts: rfka01 - Lizenz MIT }

const
  SynLoopHz = 6726.6;     { Schleifentakt: Frequenz = Schrittweite * 6726,6 / 65536 }

function SynStat: integer;
begin
  SynStat := Port[$41];
end;

function SynAvail: boolean;      { Synthesizer-Firmware vorhanden? }
var S: integer;
begin
  S := SynStat;
  SynAvail := ((S and $F0) = $C0) or ((S and $F0) = $E0) or
              ((S and $F0) = $D0) or ((S and 4) <> 0);
end;

function SynRunning: boolean;    { laeuft der Synthesizer (F0 = Bit 2)? }
begin
  SynRunning := (SynStat and 4) <> 0;
end;

function SynFree: boolean;       { Eingangspuffer des 8741 frei (IBF = 0)? }
var K: integer;
begin
  K := 0;
  while ((SynStat and 2) <> 0) and (K < 20000) do K := K + 1;
  SynFree := (SynStat and 2) = 0;
end;

procedure SynByte(B: integer);   { Datenbyte an Port 40h }
begin
  if SynFree then Port[$40] := B;
end;

procedure SynCmd(B: integer);    { Befehl an Port 41h }
begin
  if SynFree then Port[$41] := B;
end;

procedure SynReg(A, V: integer); { Register A auf V }
begin
  SynByte(A);
  SynByte(V);
end;

function SynBegin: boolean;      { Synthesizer einschalten, alle Stimmen still }
var K, I: integer;
begin
  SynBegin := false;
  if not SynAvail then exit;
  if not SynRunning then
  begin
    SynCmd($05);
    K := 0;                       { der 8741 kann noch einen Ton oder eine
                                    Taste bearbeiten (bis 0,6 s) }
    while (not SynRunning) and (K < 30000) do
    begin
      for I := 1 to 10 do ;
      K := K + 1;
    end;
  end;
  SynBegin := SynRunning;
end;

procedure SynEnd;                { Synthesizer beenden, Tastatur wieder normal }
var K: integer;
begin
  if SynRunning then SynCmd($05);
  K := 0;
  while SynRunning and (K < 30000) do K := K + 1;
end;

{ Schrittweite direkt: Stimme 1..3, Inc 0 = still. Inc als vorzeichenlose
  16-Bit-Zahl (Werte ueber 32767 als negative Integer uebergeben). }
procedure SynInc(Voice, Inc: integer);
var A: integer;
begin
  case Voice of
    1: A := $02;
    2: A := $06;
  else A := $1C;
  end;
  SynReg(A, Inc and 255);
  SynReg(A + 1, (Inc shr 8) and 255);
end;

procedure SynFreq(Voice: integer; Hz: real);   { Frequenz in Hz, 0 = still }
var R: real;
begin
  R := Hz * 65536.0 / SynLoopHz;
  if R > 65535.0 then R := 65535.0;
  if R < 0.0 then R := 0.0;
  if R >= 32768.0 then SynInc(Voice, Trunc(R - 65536.0))
  else SynInc(Voice, Trunc(R));
end;

procedure SynNoise(On: boolean); { Rauschen ein/aus }
begin
  if On then SynReg($1F, $A5) else SynReg($1F, 0);
end;

function SynClock: integer;      { Uhr im Status: 0..15, 1,28 ms je Schritt }
begin
  SynClock := SynStat shr 4;
end;

procedure SynWait(Steps: integer);   { warten, Steps x 1,28 ms (nur bei
                                       laufendem Synthesizer) }
var P, V, N: integer;
begin
  N := 0;
  P := SynClock;
  while (N < Steps) and SynRunning do
  begin
    V := SynClock;
    N := N + ((V - P) and 15);
    P := V;
  end;
end;

{ ---------------- Rennwerk (Divisionstest im 8741) ----------------
  RaceStart startet (Synthesizer muss laufen; er wird dabei verlassen).
  Ergebnis true: Rennwerk vorhanden und gestartet. Waehrend des Rennens
  liefert RacePoll den neuesten Fortschritt (0..200, -1 = nichts Neues);
  sobald der Status 60h zeigt, kommen zwei Bytes Ergebnis (lo, hi).
  Ein beliebiger Befehl an Port 41h bricht ab. }

var
  RaceLo, RaceHi, RaceGot: integer;   { Ergebnis-Sammler }

function RaceStart: boolean;
var K: integer;
begin
  RaceStart := false;
  RaceGot := 0;
  if not SynBegin then exit;
  SynReg($1A, $01);
  K := 0;
  while ((SynStat and $F4) <> $50) and (K < 30000) do K := K + 1;
  if (SynStat and $F4) = $50 then RaceStart := true
  else SynEnd;                   { nur Synthesizer, kein Rennwerk }
end;

function RacePoll: integer;      { -1 nichts, 0..200 Fortschritt, 1000 fertig }
var S, V: integer;
begin
  RacePoll := -1;
  S := SynStat;
  if (S and 1) = 0 then exit;    { OBF: kein Byte da }
  V := Port[$40];
  if ((S and $F0) = $60) or (RaceGot > 0) then
  begin
    if RaceGot = 0 then RaceLo := V else RaceHi := V;
    RaceGot := RaceGot + 1;
    if RaceGot = 2 then RacePoll := 1000;
  end
  else RacePoll := V;
end;

function RaceResult: integer;    { nach RacePoll = 1000: 1388 erwartet }
begin
  RaceResult := RaceHi * 256 + RaceLo;
end;
