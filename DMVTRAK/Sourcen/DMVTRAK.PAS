program DMVTrak;
{ DMVTRAK - MOD-Player und Tracker fuer den Lautsprecher der NCR DMV
  Turbo Pascal 3.01A, Compiler-Option C (COM-Datei).
  Klang ueber den Tastatur-Controller 8741: mit der Synthesizer-Firmware
  (Befehl 05h) drei Stimmen und Rauschen, sonst eine Stimme (Befehl 06h).
  Code: Claude (Anthropic), Idee und Prompts: rfka01 - Lizenz MIT }

{$C-}
{$R-}
{$V-}

const
  Version  = '0.4';
  MaxPat   = 100;               { Patterns 0..99 }
  MaxCh    = 8;                 { Kanaele }
  NIns     = 31;                { Instrumente 1..31 }
  Rows     = 64;                { Zeilen je Pattern }
  ScrRows  = 24;                { Bildschirmzeilen 0..23 }
  PageRows = 16;                { sichtbare Patternzeilen }
  Top      = 3;                 { erste Patternzeile am Bildschirm }
  MaxLen   = 50;                { laengster Einzelton in 20-ms-Einheiten }
  QSize    = 32;                { Tonwarteschlange }
  Unit8741 = 2048;              { 20,48 ms in 1/100 ms }

type
  Str80   = string[80];
  Str20   = string[20];
  PatBuf  = array[0..2047] of byte;   { 64 Zeilen x 8 Kanaele x 4 Byte }
  PatPtr  = ^PatBuf;
  RegPack = record
              AX, BX, CX, DX, BP, SI, DI, DS, ES, Flags: integer;
            end;

var
  { Lied }
  Title    : Str20;
  NChan    : integer;
  SongLen  : integer;
  Restart  : integer;
  NPat     : integer;
  Order    : array[0..127] of byte;
  Pat      : array[0..99] of PatPtr;
  IName    : array[1..31] of string[22];
  IPrio    : array[1..31] of integer;    { 0 = stumm, 1..9 }
  ITrans   : array[1..31] of integer;    { Halbtoene }
  IVol     : array[1..31] of integer;    { 0..64 }
  IHold    : array[1..31] of boolean;    { Ton haelt (Schleife) }
  ILen     : array[1..31] of real;       { Samplelaenge in Byte }
  INoise   : array[1..31] of boolean;    { Schlagzeug: Rauschen (Synthesizer) }
  ArpMode  : boolean;                    { false = fuehrende Stimme }
  GTrans   : integer;                    { globale Transponierung }
  InitSpd  : integer;
  InitBPM  : integer;
  Looping  : boolean;
  FileName : Str80;
  Modified : boolean;

{$I DTKSND.INC}
{$I DTKSCR.INC}
{$I DTKSONG.INC}
{$I DTKPLAY.INC}
{$I DTKSYN.INC}
{$I DTKFILE.INC}
{$I DTKEDIT.INC}

var
  P1, P2 : Str80;
  I      : integer;
  PlayOnly : boolean;

begin
  for I := 0 to MaxPat - 1 do Pat[I] := nil;
  EditActive := false;
  KbdInit;
  SndInit;
  SynInit;
  NewSong;
  PlayOnly := false;
  P1 := '';
  for I := 1 to ParamCount do
  begin
    P2 := ParamStr(I);
    if (Length(P2) = 2) and (P2[1] = '/') and (UpCase(P2[2]) = 'P') then
      PlayOnly := true
    else
      P1 := P2;
  end;
  if P1 <> '' then
    if not LoadAny(P1) then
    begin
      WriteLn('DMVTRAK: ', LastErr);
      if PlayOnly then Halt;
      NewSong;
    end;
  if PlayOnly then
    PlayerMode
  else
    Editor;
  if SynOn then SynStop;
  SndIdle;
end.
