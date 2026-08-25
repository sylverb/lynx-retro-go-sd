# Atari Lynx — Retro-Go SD dynamic core

Standalone [Handy](external/handy-go/) (handy-go) core for
[Game & Watch Retro-Go SD](https://github.com/sylverb/game-and-watch-retro-go-sd).

Build produces `lynx.bin` → `/cores/lynx.bin` on SD; ROMs go under `/roms/lynx/`
(extensions `.lnx`, `.lyx`).

## Requirements

- `arm-none-eabi-gcc` (hard-float `fpv5-d16`)
- GNU Make
- Python 3 + Pillow (`pip install -r requirements.txt`)

Or Docker: `make docker` (image `sylverb/retro-go-sd-builder:v1.5`).

**Host SDL preview** (optional, Linux / macOS)

- Native C/C++ compiler (`cc` / `c++` / clang)
- pkg-config
- SDL2 (`libsdl2-dev` / `brew install sdl2`) or SDL3 (`HOST_SDL=3`)

## Quick start

```bash
make
# → lynx.bin
```

Version in the packed header comes from `git describe --tags --dirty`
(`CORE_VERSION`; no tags → `0.0.0`). Override: `make CORE_VERSION=v1.0.0`.

## Host preview (SDL)

Desktop build of the same Handy core for faster iteration (no G&W flash
cycle). Does not replace the ARM pack for the device.

```bash
make host                       # SDL2 → ./lynx_host
make host HOST_SDL=3            # SDL3 (needs sdl3.pc)
./lynx_host                     # Esc / close window to quit
./lynx_host /path/to/game.lnx   # optional ROM (or HOST_ROM=…)
```

On macOS, if `pkg-config sdl2` fails, point it at Homebrew:

```bash
export PKG_CONFIG_PATH="$(brew --prefix)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
```

Controls: arrows = D-pad, `Z`/`X` = B/A, Enter = Start, Shift = Select,
`A`/`S` = Y/X (Option 1/2). `F1` = save state, `F2` = load state (files
under `./host_saves/`). Scale with `HOST_SCALE=2` (default).

## Memory layout

| Pool | Usage |
|------|--------|
| **ITCM** | Hot Handy code (`system`, `mikie`, `susie`) via `lynx_core.ld` |
| **DTCM** | Lynx 64 KiB system RAM (`CRam`) |
| **RAM_EMU** | Glue, cart banks, framebuffer BSS, optional ROM copy |
| **AHB** | `calloc`/`free`, small `operator new` fallback |

## SDK sync

To refresh vendored headers/bridge from firmware:

```bash
./scripts/sync_from_firmware.sh /path/to/game-and-watch-retro-go-sd
```

Template upstream: `retro-go-sd-templates` (SDK synced **2026-08-25**).

## Debug crashes

Release CI attaches a `-debug.zip` (ELF + map + `scripts/DEBUG_README.md`).
Use `scripts/resolve_addr.py` or `arm-none-eabi-addr2line` on the core ELF.

See also `CLAUDE.md` for the full memory / porting reference.
