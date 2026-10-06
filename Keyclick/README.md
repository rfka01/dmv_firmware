# Keyclick

*English below.*

## Deutsch

Die NCR Decision Mate V hat einen ziemlich nervigen Tastaturton, der mit dem originalen Tastaturcontroller auf dem Motherboard nicht abgeschaltet werden kann. Diese neue Firmware kann den Ton abschalten und auf niedrige Intensität setzen. Dazu gibt es ein Utility, das die Parameter `ON`, `OFF` und `LOW` kennt. Auf DOS, CP/M-80 und CP/M-86 ist es getestet.

### Dateien

| | |
|---|---|
| `dmv_mb_8741_32678_keyclick.bin` | neue Firmware für den Tastaturcontroller 8741 auf dem Motherboard (ersetzt ROM 32678) |
| `Images/KEYCLICK.IMG` | 360-KB-DOS-Diskette: `KEYCLICK.COM`, Beschreibung `KEYCLICK.TXT`, Quelle, `BUILD.BAT` |
| `Images/KEYCLICK_CPM.IMG` | 320-KB-CP/M-Diskette: `KEYCLICK.COM` (CP/M-80), `KEYCLICK.CMD` (CP/M-86), `KEYCLICK.68K` (CP/M-68K), `KEYCLICK.DOC`, Quellen |
| `Sourcen/Firmware/` | `MKROM.PY` erzeugt die Firmware aus dem Original-ROM 32678, `PATCH.LST` zeigt alle Änderungen |
| `Sourcen/DOS/` | MS-DOS-Version (MASM 5.10) |
| `Sourcen/CPM/` | CP/M-80 (8080, `ASM`/`LOAD`), CP/M-86 (MASM-Syntax + `MKCMD.PY`), CP/M-68K (GNU as + `MK68K.PY`) |

### Aufruf

```
KEYCLICK        zeigt die Einstellung
KEYCLICK ON     Tastaturklick an
KEYCLICK OFF    Tastaturklick aus
KEYCLICK LOW    nur ein leiser Knack
```

Unter DOS liefert KEYCLICK einen ERRORLEVEL: 0 erledigt, 1 falscher Parameter, 2 Originalfirmware, 3 keine Antwort, 4 erste Patch-Version ohne LOW. Nach jedem Reset ist der Klick wieder an; wer ihn nie hören will, schreibt `KEYCLICK OFF` in die AUTOEXEC.BAT. Bell und Musik (`ESC M`) bleiben unverändert.

### Wie es funktioniert

Den Klick erzeugt nicht die Tastatur, sondern der Lautsprecher auf dem Motherboard, den der 8741 über P2.0 ansteuert. Die Originalfirmware spielt jedes Mal, wenn sie einen Tastencode an den Rechner weitergibt, einen Ton von etwa 870 Hz und 20 ms – ohne Möglichkeit, ihn abzuschalten. Die neue Firmware nutzt freie Bereiche des ROMs und die freien Befehle des Controllers:

- `02h` Klick aus, `03h` Klick an, `04h` LOW: statt des Tons ein einzelner Impuls von 0,5 ms
- Statusregister (Port 41h): Bit 6 = neue Firmware vorhanden, Bit 5 = aus, Bit 4 = LOW
- Die Einstellung liegt im RAM des 8741; der Selbsttest beim Einschalten setzt sie zurück.

Die Originalfirmware bleibt bei den Befehlen `02h` bis `05h` in ihrem Befehlsverteiler hängen und fragt die Tastatur nicht mehr ab. KEYCLICK prüft deshalb zuerst Bit 6 und schickt ohne neue Firmware nichts.

### Einbau

- **MAME:** Die Datei an Stelle von `dmv_mb_8741_32678.bin` laden, z. B. lokal in `dmv.cpp`:
  `ROMX_LOAD( "dmv_mb_8741_32678_keyclick.bin", 0x00000, 0x00400, CRC(82709883) SHA1(dbb69f0cd272f5814a1eefbce1fceea45c701edb), ROM_BIOS(n) )`
- **Echte DMV:** Der Tastaturcontroller ist ein 8741(A) mit EPROM. Er muss mit der neuen Firmware programmiert werden; das können nicht alle Programmiergeräte.

Die Firmware gilt für ROM 32678. Ältere Motherboards mit ROM 32121 brauchen eine eigene Anpassung.

### Test

DOS, CP/M-80 und CP/M-86 sind in MAME getestet. KEYCLICK.68K für CP/M-68K ist gebaut und nur in einer Emulation von 68000 und 8741 geprüft, weil CP/M-68K auf der DMV derzeit nicht zuverlässig läuft.

## English

The NCR Decision Mate V has a rather annoying key click that cannot be switched off with the original keyboard controller on the motherboard. This new firmware can switch the click off or set it to low intensity. A utility that knows the parameters `ON`, `OFF` and `LOW` comes with it. It has been tested under DOS, CP/M-80 and CP/M-86.

### Files

| | |
|---|---|
| `dmv_mb_8741_32678_keyclick.bin` | new firmware for the 8741 keyboard controller on the motherboard (replaces ROM 32678) |
| `Images/KEYCLICK.IMG` | 360 KB DOS disk: `KEYCLICK.COM`, description `KEYCLICK.TXT` (German), source, `BUILD.BAT` |
| `Images/KEYCLICK_CPM.IMG` | 320 KB CP/M disk: `KEYCLICK.COM` (CP/M-80), `KEYCLICK.CMD` (CP/M-86), `KEYCLICK.68K` (CP/M-68K), `KEYCLICK.DOC` (German/English), sources |
| `Sourcen/Firmware/` | `MKROM.PY` builds the firmware from the original ROM 32678, `PATCH.LST` lists all changes |
| `Sourcen/DOS/` | MS-DOS version (MASM 5.10) |
| `Sourcen/CPM/` | CP/M-80 (8080, `ASM`/`LOAD`), CP/M-86 (MASM syntax + `MKCMD.PY`), CP/M-68K (GNU as + `MK68K.PY`) |

### Usage

```
KEYCLICK        show the current setting
KEYCLICK ON     key click on
KEYCLICK OFF    key click off
KEYCLICK LOW    just a faint tick
```

Under DOS KEYCLICK returns an ERRORLEVEL: 0 done, 1 wrong parameter, 2 original firmware, 3 no answer, 4 first patch version without LOW. The click is on again after every reset; put `KEYCLICK OFF` into AUTOEXEC.BAT to never hear it. Bell and music (`ESC M`) are not affected.

### How it works

The click is not made by the keyboard but by the speaker on the motherboard, which the 8741 drives through P2.0. The original firmware plays a tone of about 870 Hz for 20 ms whenever it passes a key code to the computer, with no way to turn it off. The new firmware uses unused areas of the ROM and the free commands of the controller:

- `02h` click off, `03h` click on, `04h` LOW: a single 0.5 ms pulse instead of the tone
- status register (port 41h): bit 6 = new firmware present, bit 5 = off, bit 4 = LOW
- The setting lives in the 8741 RAM; the self test at power-on resets it.

With commands `02h` to `05h` the original firmware hangs in its command dispatcher and stops scanning the keyboard. KEYCLICK therefore checks bit 6 first and sends nothing without the new firmware.

### Installation

- **MAME:** load the file instead of `dmv_mb_8741_32678.bin`, e.g. locally in `dmv.cpp`:
  `ROMX_LOAD( "dmv_mb_8741_32678_keyclick.bin", 0x00000, 0x00400, CRC(82709883) SHA1(dbb69f0cd272f5814a1eefbce1fceea45c701edb), ROM_BIOS(n) )`
- **Real DMV:** the keyboard controller is an 8741(A) with EPROM. It has to be programmed with the new firmware; not every programmer supports it.

The firmware is for ROM 32678. Older motherboards with ROM 32121 need their own adaptation.

### Tests

DOS, CP/M-80 and CP/M-86 have been tested in MAME. KEYCLICK.68K for CP/M-68K is built and only checked in an emulation of the 68000 and the 8741, because CP/M-68K does not run reliably on the DMV at the moment.

---

MIT License, see [LICENSE](../LICENSE). Code: Claude (Anthropic) · Prompts: rfka01
