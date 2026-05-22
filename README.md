# Captain Comic Modernization

A faithful modern recreation of the 1988 DOS game *The Adventures of Captain Comic* built with SDL2, C++17, and CMake.

![Captain Comic Title](images/title.png)

## What This Is

This project ports the original game to modern platforms while keeping the old gameplay rules, timing, and stage structure intact. The codebase uses the C refactor by jsandas as the primary reference, with the original assembly and game assets used when exact behavior or data is needed.

Current work is focused on release preparation and documentation. The core game systems are already in place:

- Player physics, collisions, and stage transitions
- Tile, sprite, and camera rendering
- Level loading for all 8 worlds
- Enemies, fireballs, and items
- Audio, title sequence, HUD, and menus
- Debug cheats for development

## Quick Start

If you already have the required SDL2 libraries and the original game data, the shortest path is:

```bash
cmake --preset default
cmake --build --preset build-default
./build/captain_comic
```

On first setup, you also need to extract the original game assets into the local `original/` folder and run the asset conversion script described below.

## Requirements

- CMake 3.23+
- A C++17 compiler
- SDL2 development packages
- SDL2_image development packages
- SDL2_ttf development packages
- SDL2_mixer development packages are recommended for audio output
- Python 3 for the asset extraction tool
- The original Captain Comic R5 game archive

## Assets

The game does not ship copyrighted assets. You need the original Captain Comic files to build the converted graphics, maps, sprites, and sounds used by the modern port.

1. Download the R5 archive and unpack it into `original/`.

```bash
curl -L -o comic_r5.zip \
  https://archive.org/download/TheAdventuresOfCaptainComic/AdventuresOfCaptainComicEpisode1The-PlanetOfDeathR5sw1991michaelA.Denioaction.zip
unzip comic_r5.zip -d original
rm comic_r5.zip
```

2. Install the Python dependencies and run the extractor.

```bash
pip install -r tools/requirements.txt
python3 tools/extract_assets.py --orig original
```

After extraction, `assets/` contains the converted PNG, GIF, map, and sound files used by the game.

## Build

### macOS

```bash
brew install cmake pkg-config sdl2 sdl2_image sdl2_ttf sdl2_mixer python
cmake --preset default
cmake --build --preset build-default
```

### Linux

```bash
sudo apt-get install cmake g++ pkg-config libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev python3 python3-pip
cmake --preset default
cmake --build --preset build-default
```

### Windows

Use vcpkg, Visual Studio, or another CMake-capable toolchain with SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer available. The documented and tested vcpkg setup uses the `x64-windows-static` triplet.

```bash
vcpkg install --triplet x64-windows-static sdl2 sdl2-image sdl2-ttf sdl2-mixer
cmake --preset default
cmake --build --preset build-default
```

## Run

**From a release binary:** Download the archive for your platform from the [GitHub Releases](https://github.com/jsandas/captain-comic-refactor/releases) page. Windows and macOS binaries are self-contained — no SDL2 installation needed. Linux binaries link against system SDL2 and require the runtime libraries:

```bash
sudo apt-get install libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-ttf-2.0-0 libsdl2-mixer-2.0-0
```

**From a local build:** Launch from the build directory:

```bash
./build/captain_comic
```

Command-line flags:

- `--help` shows the available flags
- `--skip-title` skips the title and startup sequence
- `--debug` enables cheat keys and debug overlay controls

## Controls

Gameplay controls are intentionally close to the original DOS layout:

- Left and right arrows move Comic
- Up arrow is used for ladder and context-sensitive movement where applicable
- Space jumps
- Left Ctrl fires
- Alt opens doors
- Escape pauses or backs out of menus

Debug controls are available only with `--debug`:

- F1 toggles noclip
- F2 opens level warp
- F3 toggles the debug overlay
- F4 opens position warp
- F5 opens item granting

The title flow also exposes menu screens for startup notice, high scores, and keyboard setup.

## Testing

The workspace includes a separate test binary.

```bash
cmake --build --preset build-default --target comic_tests
./build/comic_tests --list
./build/comic_tests --filter physics
```

The tests cover physics, rendering helpers, actors, audio, UI, and score handling.

## Features

- Faithful 320x200 presentation with letterboxing
- 8 playable levels with 3 stages each
- Door logic, checkpoints, and level transitions
- Five enemy AI types and respawn behavior
- Item effects including firepower, boots, lantern, shield, key, teleport wand, and treasures
- PC-speaker-style synthesized audio and looping title music
- HUD with score, lives, meters, and inventory grid
- Menus and special sequences including beam-in, beam-out, death, and victory
- Development cheats and debug overlay

## Project Layout

- `src/` main game systems and executable entry point
- `include/` shared headers
- `tests/` unit and integration tests
- `assets/` converted runtime assets
- `original/` original DOS data used by the extraction tool
- `tools/` asset extraction scripts and helper files
- `docs/` roadmap and planning documents

## Roadmap

See [docs/MODERNIZATION_PLAN.md](docs/MODERNIZATION_PLAN.md) for the full implementation plan and current phase breakdown.

## Credits

- Original game: *The Adventures of Captain Comic* by Michael A. Denio
- Reference implementation: [jsandas/comic-c](https://github.com/jsandas/comic-c)
- SDL2, SDL2_image, SDL2_ttf, and SDL2_mixer for platform support

## Notes

- The game expects converted assets to exist under `assets/` before launch.
- SDL2_mixer is recommended for audio, but the build system can still configure without it.
- If you are packaging the project, keep the original game data outside the repository unless your distribution rights allow otherwise.