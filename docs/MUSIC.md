# Replacement music

*Ultima Underworld*'s port (UW1) can play your own music files in place of the game's music, one file for each of the game's themes. The files are **Ogg Vorbis** (`.ogg`), a free and open format with no patents. Themes you give no file for play the game's own music as before.

## Using it

1. Put the files in a folder called `music` in the port's home folder (`~/.uw1port/music` on macOS and Linux, `%APPDATA%\uw1port\music` on Windows), or anywhere else and choose that folder on the settings screen's Sound tab (Music folder).
2. Name each file after the theme it replaces, below: `uw02.ogg` plays in place of the first walking theme, and so on. Lower case or all capitals (`UW02.OGG`) both work.
3. Play. The next time the game starts a theme you have a file for, your file plays.

On the settings screen's **Sound** tab ([SETTINGS.md](SETTINGS.md)):

| Row | What it does | Settings-file key | Default |
|---|---|---|---|
| Replacement music | On: a theme with a file plays the file. Off: the game's own music everywhere | `music-replace` | on |
| Music folder | The folder the files are in. The row says how many of the 13 themes it has a file for, by either name (counted again every two seconds while the screen is open), or "not in this build" | `music-dir` | `music` in the home folder |
| Replacement volume | The files' volume, 0 to 100 (the Volume row above still scales everything) | `music-volume` | 70 |

The three rows take effect as the settings screen closes: the theme playing takes its file from a new folder at once, and the switch and the volume apply at once too. They are not on the web page, which has no folders of yours to read.

## The themes

The game's themes are its music files, `SOUND\UWnn.XMI`, where `nn` is the theme's number in two octal digits. A replacement file has the same name with `.ogg`.

| File | Theme | When the game plays it |
|---|---|---|
| `uw01.ogg` | 1 | the start-up and the main menu |
| `uw02.ogg`, `uw03.ogg`, `uw04.ogg` | 2 to 4 | walking: one of the three at random, and again at random each time one ends |
| `uw05.ogg` | 5 | a creature you hit is below a quarter of its hit points |
| `uw06.ogg` | 6 | combat: a creature attacks you, or is hit |
| `uw07.ogg` | 7 | danger: you are below a quarter of your hit points |
| `uw10.ogg` | 8 | your weapon is drawn |
| `uw11.ogg` | 9 | a creature is killed (plays to its end) |
| `uw12.ogg` | 10 | you die |
| `uw13.ogg` | 11 | plays to its end; where the game asks for it is not yet identified |
| `uw14.ogg` | 12 | not yet identified |
| `uw15.ogg` | 13 | conversations, the map and sleep |

On the FM cards (Ad Lib, Sound Blaster, Pro Audio Spectrum) the game plays its own FM arrangements, `AWnn.XMI`. On those cards an `awnn.ogg` is used before `uwnn.ogg`, so a set of files can give a version for each kind of card. Most sets will have only `uwnn.ogg`, which then serve every card.

## How it plays

- **The game's own music still runs, unheard.** The game decides when a theme starts, ends or changes by asking its music driver, so the driver plays the theme as always, and while your file plays the theme's notes only go unheard: the synthesiser still counts each one against its cache of instruments, exactly as it would to play it, since the game reads that cache back. The game's music logic and its timing are exactly the original's, so a session recorded with replacement music replays exactly, in DOS too. The sound effects, which the game plays on the same synthesiser, are still heard.
- **A file plays from the theme's start and loops** for as long as the theme is the game's. The game starts a theme again each time the original ends; that does not restart your file, so a file need not be as long as the original.
- **A new theme cross-fades**: the old file fades out over half a second while the new one comes in, however quickly the game changes themes. A theme with no file stops the file and brings back the game's own music.
- **When the game stops its music** (music off on the game's options panel, a cutscene), the file fades out at once. When a theme only ends and the game does not start one again within five seconds, it fades out too.
- **Any sample rate, mono or stereo.** Each file is resampled to the port's 44.1 kHz.
- The whole file loops: there are no loop points, so a file with an introduction plays it again each time round.
- A file that is not Ogg Vorbis is passed over, with a line in the log, and that theme plays the game's own music. The log also names each file as it starts playing (`music: FILE ... in place of the game's theme`).

To make a file from another format, `ffmpeg -i track.flac -c:a libvorbis -q:a 6 uw02.ogg` or `oggenc -q 6 -o uw02.ogg track.wav`.

## Building it

The decoder is stb_vorbis (Sean Barrett, public domain or MIT), fetched with the port's other sound libraries by `make setup-sound` (or `make setup-port`) into `uw1/tools/stb-vorbis` and never committed (when it cannot be fetched, setup says so and goes on) ([THIRD-PARTY-NOTICES](../uw1/THIRD-PARTY-NOTICES)). A port built without it has no replacement music, and the Music folder row says "not in this build". The mechanism is Exhume's `runtime/port/sound/altmusic.c`, and its host tests are `runtime/tests/altmusic_test.c`, which plays generated files through it, and `runtime/tests/silentnote_test.c`, which checks that the synthesisers' instrument caches stay as they would with the notes heard.
