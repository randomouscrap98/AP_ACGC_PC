# Animal Crossing Archipelago Setup Guide

## Required Software

- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.7 or newer
- The latest release of this project from the [releases page](https://github.com/randomouscrap98/AP_ACGC_PC/releases):
  - `AnimalCrossingAP-<version>.zip` (the game)
  - `animal_crossing.apworld`
- An Animal Crossing (USA, GAFE01 Rev 0) disc image in ISO, GCM or CISO format

The game is a Windows program. On Linux/macOS it runs under Wine.

## Installation

1. Install `animal_crossing.apworld` by copying it into the
   `custom_worlds` folder of your Archipelago install.
2. Extract `AnimalCrossingAP-<version>.zip` anywhere.
3. Put your disc image in the `rom` folder next to `AnimalCrossing.exe`.

## Creating Your Options File

Use the Options Creator inside the Archipelago Launcher. Set a slot
name, modify the settings of your choice, and export. Send to
the host, or put it in your `Archipelago/Players` folder for a local generation.

## Connecting

Create a file named `ap_config.ini` next to `AnimalCrossing.exe`:

```ini
host=archipelago.gg:12345
slotname=YourSlotName
password=
```

- `host`: the server address and port. Use `ws://` for a local server (e.g. `ws://localhost:38281`).
- `slotname`: the name from your options file.
- `password`: leave empty if the room has none.

Then run `AnimalCrossing.exe`. The game connects on startup and shows its connection status
on screen.

## Playing Without a Server

To play with only the quality-of-life options (no multiworld), replace the contents of
`ap_config.ini` with:

```ini
offline=offline_example.json
```

and edit `offline_example.json` to choose your options.
