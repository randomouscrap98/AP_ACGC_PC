# Animal Crossing Archipelago Setup Guide

## Required Software

- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.7 or newer
- The latest release of this project from the [releases page](https://github.com/randomouscrap98/AP_ACGC_PC/releases):
  - `AnimalCrossingAP-<version>.zip` (the game, Windows) or `AnimalCrossingAP-<version>-linux.tar.gz` (the game,
    native Linux, see below)
  - `animal_crossing.apworld`
- An Animal Crossing (USA, GAFE01 Rev 0) disc image in ISO, GCM or CISO format

On macOS the Windows version runs under Wine. On Linux you can use the native Linux version, or the Windows
version under Wine/Proton.

## Installation

1. Install `animal_crossing.apworld` by copying it into the
   `custom_worlds` folder of your Archipelago install.
2. Extract `AnimalCrossingAP-<version>.zip` (or the Linux `.tar.gz`) anywhere.
3. Put your disc image in the `rom` folder next to `AnimalCrossing.exe` (Linux: `AnimalCrossing`).

### Linux version

The Linux version is a 32-bit program. It needs glibc 2.36 or newer (Ubuntu 24.04, Debian 12, Fedora 37, or
newer) and these 32-bit system libraries; everything else is built in:

- the OpenGL driver for your graphics card
- X11 (the game also runs on Wayland desktops through XWayland)
- PulseAudio or ALSA, for sound (without them the game runs silently). PipeWire desktops use the PulseAudio one.

If Steam is installed, you most likely have them already. Otherwise:

- Arch (enable the `multilib` repository first): `sudo pacman -S lib32-mesa lib32-libx11 lib32-libxext lib32-libpulse lib32-alsa-lib`;
  with an NVIDIA driver also `lib32-nvidia-utils`
- Debian/Ubuntu: `sudo dpkg --add-architecture i386 && sudo apt update`, then
  `sudo apt install libgl1:i386 libgl1-mesa-dri:i386 libx11-6:i386 libxext6:i386 libpulse0:i386 libasound2:i386`;
  (Ubuntu 24.04 and newer: `libasound2t64:i386` instead of `libasound2:i386`); with an NVIDIA driver also its
  matching `:i386` package (e.g. `libnvidia-gl-<version>:i386`)
- Fedora: `sudo dnf install mesa-libGL.i686 mesa-dri-drivers.i686 libX11.i686 libXext.i686 pulseaudio-libs.i686 alsa-lib.i686`

Start the game with `./AnimalCrossing` from its folder (or double-click it). On Wayland, run
`./install-desktop-entry.sh` once to add it to your app launcher with its icon.

## Creating Your Options File

Use the Options Creator inside the Archipelago Launcher. Set a slot
name, modify the settings of your choice, and export. Send to
the host, or put it in your `Archipelago/Players` folder for a local generation.

## Connecting

The first time the game runs it creates `ap_config.ini` next to `AnimalCrossing.exe` (Linux: `AnimalCrossing`), and
the title screen says `EDIT ap_config.ini: fill in host and slotname`. Fill it in and restart the game:

```ini
host = archipelago.gg:12345
slotname = YourSlotName
password =
```

- `host`: the server address and port. Use `ws://` for a local server (e.g. `ws://localhost:38281`).
- `slotname`: the name from your options file.
- `password`: leave empty if the room has none.

Then run the game. It connects on startup and shows its connection status
on screen. If something in the file is wrong, the status line says what (`EDIT ap_config.ini: ...`) and Start
Game stays blocked until it's fixed and the game restarted. Lines starting with `;` are comments.

### AP Client

The game has a built-in client, letting you send commands and easily track your
received items, hints, and checks left to do. It shows up on the PC pause menu,
in the upper left corner. The client can be expanded or collapsed, moved and
resized.

If for some reason your game crashes when opening the PC pause menu, you can
turn off the built-in client entirely by going to "Options" in the title menu. 
Under "Video", you'll find an option to disable the built-in client.

## Playing Without a Server

To play with only the quality-of-life options (no multiworld), replace the contents of
`ap_config.ini` with:

```ini
offline=offline_example.json
```

and edit `offline_example.json` to choose your options. Options that are also in the in-game Settings > AP tab are only read when a
save is first loaded; after that, change them in game (editing the file does nothing for that save).
