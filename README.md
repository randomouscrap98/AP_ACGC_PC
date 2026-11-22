# Animal Crossing PC Port + Archipelago

A fork of the [Animal Crossing PC Port](https://github.com/flyngmt/ACGC-PC-Port) which
adds MANY changes for Archipelago support. This is a full port with full Archipelago
support; you do not need a separate client or mod.

This repository does not contain any game assets or assembly whatsoever. An existing copy of the game is required.

Supported versions: GAFE01_00: Rev 0 (USA)

## Setup
- Setup guide: [docs/setup_en.md](docs/setup_en.md)
- Game info: [docs/game_info_en.md](docs/game_info_en.md)

## Building from Source

### Requirements

- **podman**
- **Animal Crossing (USA) disc image** (ISO, GCM, or CISO format)
- Linux build only: glibc 2.36+ and 32-bit OpenGL, X11 and PulseAudio/ALSA libraries (already
  installed if you have Steam; package names per distro in [docs/setup_en.md](docs/setup_en.md#linux-version))

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/randomouscrap98/AP_ACGC_PC.git
   cd AP_ACGC_PC
   ```

2. Build (from a bash shell):

   | | Windows | Linux |
   |---|---|---|
   | Command | `./build_podman.sh` | `./build_podman.sh --linux` |
   | Output | `build/windows32/bin/` | `build/linux32/bin/` |
   | Run | `AnimalCrossing.exe` | `AnimalCrossing` |

3. Put your disc image in the output folder's `rom/` folder, e.g. `build/windows32/bin/rom/YourGame.ciso`.

4. Run the game from the output folder. On Linux with Wayland, run `install-desktop-entry.sh` there once
   for a launcher entry and taskbar icon.

## Controls

Keyboard bindings are customizable via `keybindings.ini` (next to the executable). Mouse buttons (Mouse1/Mouse2/Mouse3) can also be assigned.

### Keyboard (defaults)

| Key | Action |
|-----|--------|
| WASD | Move (left stick) |
| Arrow Keys | Camera (C-stick) |
| Space | A button |
| Left Shift | B button |
| Enter | Start |
| X | X button |
| Y | Y button |
| Q / E | L / R triggers |
| Z | Z trigger |
| I / J / K / L | D-pad (up/left/down/right) |

### Gamepad

SDL2 game controllers are supported with automatic hotplug detection. Button mapping follows the standard GameCube layout.

## Command Line Options

| Flag | Description |
|------|-------------|
| `--verbose` | Enable diagnostic logging |
| `--no-framelimit` | Disable frame limiter (unlocked FPS) |
| `--model-viewer [index]` | Launch debug model viewer (structures, NPCs, fish) |
| `--sound-test` | Launch the sound test (songs, loops, effects) instead of the game |
| `--crash-test` | Crash on purpose after startup, to check the crash report |
| `--help` | List every option |

Debug only (they bypass Timesanity and the other time rules): `--time H[:M[:S]]` and `--date M/D[/Y]` override
the in-game time and date.

## Settings

Graphics settings are stored in `settings.ini` and can be edited manually or through the in-game options menu:

- Render scale (Auto by default, 1x-6x, or Window): the game draws at a multiple of 480 lines and is scaled
  down to the window. This is the quality setting.
- Fullscreen (desktop resolution), Borderless or Windowed (remembers the size you drag the window to)
- VSync
- MSAA (anti-aliasing; off by default, applies right away)
- Texture Loading/Caching (No need to enable if you aren't using a texture pack)
- Audio: Master, Music and Sound effects volume (Music 0% mutes the music; NES games follow Master only)

### AP settings

Archipelago connection settings, such as the host, slotname, and password, go into a separate
`ap_config.ini` file next to the exe. There is no in-game way to set these values (to keep
things simple; Animal Crossing does not have a robust text entry builtin). The game creates the
file on first run; fill it in and restart. See [docs/setup_en.md](docs/setup_en.md)

## Texture Packs

Custom textures can be placed in `texture_pack/`. Dolphin-compatible format (XXHash64, DDS).

The original PC port author highly recommended the following texture pack from the 
talented artists of Animal Crossing community.

[HD Texture Pack](https://forums.dolphin-emu.org/Thread-animal-crossing-hd-texture-pack-version-23-feb-22nd-2026)

Leave Settings > Video > Textures on "On Demand" (the default) unless you get stutter. "Preload" and
"Preload&Cache" load the whole pack at startup, and with a big pack like the one above that can crash at
startup on some graphics drivers (the game is a 32-bit program and can run out of memory).

## Save Data

Save files are stored in `saves/` using the standard GCI format, compatible with Dolphin emulator saves. 

Each slot saves to a separate folder, so you don't have to worry about save management per
session. The folder name is the "seed" you see on the title screen, if you need to find a save
for a specific session.

## Offline mode

Normally, you cannot start a game until you connect to Archipelago. This is required to get
information from the server for your save file. However, if you are finished with your
multiworld but still want to play on your old save file, you can do this with "offline mode".
It also lets you play with only the quality-of-life options, without a multiworld. See
[Playing Without a Server](docs/setup_en.md#playing-without-a-server) in the setup guide.

## AI Notice

Original notice retained from parent of fork (https://github.com/flyngmt/ACGC-PC-Port):

> AI tools such as Claude were used in this project (PC port code only).
