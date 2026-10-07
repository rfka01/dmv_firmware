# DMVTRAK – MOD-Player und Tracker

*English below.*

## Deutsch

DMVTRAK spielt Amiga-MODs auf dem Lautsprecher der NCR Decision Mate V und ist zugleich ein kleiner Tracker zum Bearbeiten und Schreiben eigener Stücke. Geschrieben in Turbo Pascal 3.01A für MS-DOS.

- **Mit der Synthesizer-Firmware** ([`../SYNDEMO`](../SYNDEMO)) erzeugt der Tastaturcontroller 8741 drei Rechteckstimmen und Rauschen. DMVTRAK verteilt die drei wichtigsten Kanäle auf die Stimmen, stellt die Tonhöhe exakt aus der MOD-Periode (Slides und Arpeggio stufenlos) und nimmt das Rauschen für Schlagzeug. Den Takt liefert der Timer des 8741.
- **Mit der Originalfirmware** klingt eine Stimme in Halbtönen; DMVTRAK spielt dann die führende Stimme oder wechselt die Kanäle im Arpeggio.

MODs werden ohne Samples geladen (15 oder 31 Instrumente, 4/6/8 Kanäle). Gespeichert wird im eigenen, kleinen Format DTK, das Prioritäten und Rauschen der Instrumente mitführt.

### Dateien

| | |
|---|---|
| `Images/DMVSOUND.IMG` | 360-KB-DOS-Diskette: `DMVTRAK.COM`, `DMVTRAK.TXT`, `SYNDEMO.COM`, `SYNTH.TXT`, Musik `AXELF.DTK`, `JARRE.DTK`, `POPCORN.MOD` |
| `Sourcen/` | `DMVTRAK.PAS` (Hauptdatei) und `DTK*.INC`; `DMVTRAK.TXT` Beschreibung mit allen Tasten |

Die Firmware liegt in [`../SYNDEMO`](../SYNDEMO) (`dmv_mb_8741_32678_synth.bin`).

### Aufruf

```
DMVTRAK                 Editor mit leerem Lied
DMVTRAK datei           Editor mit MOD oder DTK
DMVTRAK datei /P        nur abspielen (Leertaste oder ESC = Ende)
```

Im Editor öffnet ESC das Menü (Laden, Speichern, Instrumente, Reihenfolge, Modus, Tempo …); die Leertaste spielt ab Cursor, RETURN von vorn. Alle Tasten stehen in `DMVTRAK.TXT`.

### Neu bauen

Turbo Pascal 3.01A, `DMVTRAK.PAS` als Hauptdatei, Compiler-Option C (Com-file), die Dateien `DTK*.INC` im selben Verzeichnis. Die Quelltexte haben CRLF-Zeilenenden.

### Test

Im Emulator (emu2 mit einem Modell des 8741) und mit der zyklengenauen Simulation des 8741 geprüft; von rfka01 erprobt.

### Musik

`AXELF`, `JARRE` und `POPCORN` sind MODs anderer Autoren und fallen nicht unter die MIT-Lizenz.

## English

DMVTRAK plays Amiga MODs on the speaker of the NCR Decision Mate V and is also a small tracker for editing and writing your own pieces. Written in Turbo Pascal 3.01A for MS-DOS.

- **With the synthesizer firmware** ([`../SYNDEMO`](../SYNDEMO)) the 8741 keyboard controller generates three square-wave voices and noise. DMVTRAK assigns the three most important channels to the voices, sets the pitch exactly from the MOD period (smooth slides and arpeggio) and uses the noise for drums. The 8741 timer provides the tempo.
- **With the original firmware** a single voice plays in semitones; DMVTRAK then plays the leading voice or alternates the channels as an arpeggio.

MODs are loaded without samples (15 or 31 instruments, 4/6/8 channels). Songs are saved in a small format of its own, DTK, which keeps the priorities and noise settings of the instruments.

### Files

| | |
|---|---|
| `Images/DMVSOUND.IMG` | 360 KB DOS disk: `DMVTRAK.COM`, `DMVTRAK.TXT` (German), `SYNDEMO.COM`, `SYNTH.TXT`, music `AXELF.DTK`, `JARRE.DTK`, `POPCORN.MOD` |
| `Sourcen/` | `DMVTRAK.PAS` (main file) and `DTK*.INC`; `DMVTRAK.TXT` description with all keys (German) |

The firmware is in [`../SYNDEMO`](../SYNDEMO) (`dmv_mb_8741_32678_synth.bin`).

### Usage

```
DMVTRAK                 editor with an empty song
DMVTRAK file            editor with a MOD or DTK
DMVTRAK file /P         play only (space or ESC = end)
```

In the editor ESC opens the menu (load, save, instruments, order, mode, tempo …); space plays from the cursor, RETURN from the start. All keys are listed in `DMVTRAK.TXT`.

### Building

Turbo Pascal 3.01A, `DMVTRAK.PAS` as main file, compiler option C (com file), the `DTK*.INC` files in the same directory. The sources have CRLF line endings.

### Tests

Checked in an emulator (emu2 with a model of the 8741) and with the cycle-accurate 8741 simulation; tried out by rfka01.

### Music

`AXELF`, `JARRE` and `POPCORN` are MODs by other authors and are not covered by the MIT license.

---

MIT License, see [LICENSE](../LICENSE). Code: Claude (Anthropic) · Prompts: rfka01
