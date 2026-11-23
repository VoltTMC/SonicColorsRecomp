# A3y — Loose SFD movie / cutscene replacements

Replace Sonic Colors Wii cutscenes with your own `.sfd` files **without** modifying the extracted game data or rebuilding a CPK.

## Where originals live

Movies are **loose disc files**, not members of `sonic2010_0.cpk`:

```
<game DATA>/files/movie/*.sfd
```

Example (Europe / SNCP8P install):

```
game/SNCP8P/files/movie/ev110.sfd
game/SNCP8P/files/movie/opening_eu.sfd
game/SNCP8P/files/movie/segalogo_us.sfd
…
```

There are **34** `.sfd` files on SNCP8P. The directory name is singular **`movie`**, not `movies`.

## How the recomp loads movies

```
Game cutscene (CMoviePlayer / mwPlyCreateSofdec)
  → builds path movie/<name>.sfd
  → DVD / FST read (Disc_ReadPartition)
  → host file under Data/files/movie/  (or a registered override)
  → CRI Sofdec MPEG-PS demux + decode (game code)
  → GX present + ADX audio
```

The original Sofdec player is already in the recompiled game. A3y only adds a **host override** so replacement files can differ in size from the FST entry (FST offset/size are patched in memory, same mechanism as folder mods and music CPK redirects).

**Do not** overwrite `Data/files/movie/*.sfd` with larger HD files by hand: the on-disc FST still reports the original size, so the game would only read a prefix of your file. Use the override folder (or a proper mod) instead.

## Folder to create (recommended)

Drop replacements into your **user data** movie folder (created automatically on first run if missing):

| Install type | Path |
|---|---|
| Windows installer | `%APPDATA%\SonicColorsRecomp\movie\` |
| Linux AppImage | `~/.local/share/SonicColorsRecomp/movie/` |
| Portable / `--portable` | `<folder next to the exe>/movie/` |

Put the `.sfd` files **directly** in that folder, using the **exact original filenames**:

```
movie/
  ev110.sfd
  ev350.sfd
  opening_eu.sfd
  segalogo_us.sfd
```

Also accepted inside the same root:

- `movie/ev110.sfd` (nested mirror)
- `files/movie/ev110.sfd` (mod-style mirror)

CLI override: `--movie-mods /path/to/dir` or config `[Movie] movie_dir = …`.

## Alternative: Mods tab package

A normal game mod already works if enabled:

```
mods/ReRainbowCutscenes/
  mod.ini          (optional)
  files/
    movie/
      ev110.sfd
      …
```

Host overrides from `userdata/movie/` win over folder mods when both replace the same file (same precedence as music CPK host overrides).

## Why `Data\files\movies` did nothing

Issue #2 used **`movies`** (plural) next to a texture pack. The disc FST path is **`files/movie/`** (singular). A random `Data/files/movies` folder is not part of the FST and is never opened.

## Supported format

Originals (and working replacements) are **CRI Sofdec** streams: MPEG program stream with **MPEG-1 video** (~640×480, ~29.97/30 fps) and **ADX** audio (often 32 kHz stereo; some events have two ADX streams).

Replacements must:

- Keep the **same filename** as the movie they replace.
- Be a valid MPEG-PS / Sofdec SFD (pack/system/padding start codes).
- Stay within a 32-bit FST size (under 4 GiB).

Larger or smaller than the original is fine — the in-memory FST is updated.

## Audio / language notes

- Embedded ADX in the SFD is what Sofdec plays; do not expect a separate PC audio file.
- Some originals have **two** ADX streams (e.g. language variants). Replacements may use different sample rates (e.g. 44.1 kHz) or stream counts — the game player may still accept them, but language selection behaviour can differ from stock.
- Subtitles, when used, come from game UI/scripts, not from the SFD container.

## Install / remove

1. Copy `.sfd` files into `…/movie/` with original names.
2. Start the game. Log lines: `[MOVIE] override movie/….sfd → …`.
3. To restore an original: delete that replacement file (or disable `[Movie] enabled = false`) and restart.

Unused filenames are logged as unused (not silently mapped to another cutscene). Invalid headers are rejected; the original disc file remains the fallback for that slot.

## Troubleshooting

| Symptom | Check |
|---|---|
| Still see originals | Filename must match exactly; folder must be `movie` not `movies`; restart after copying |
| Black / error cutscene | SFD not Sofdec/MPEG-PS, or dimensions exceed Sofdec create params — see log |
| Truncated video after overwriting game files | Restore originals from your disc dump; use `userdata/movie/` overrides instead |
| Mod package ignored | Enable the mod in the Mods tab; layout must include `files/movie/` |

Config (`config.ini`):

```ini
[Movie]
enabled = true
movie_dir =
```

## Related

- Music replacements (CPK): `docs/MUSIC_MODDING.md`
- Issue #2: loose HD cutscenes from Project Re-Rainbow (user-supplied SFDs; **not** redistributed by this repo)
