## General

This document describes a bunch of stuff related to the DEVELOPMENT of the Archipelago 
fork of the ACGC-PC port.

- Forked from a port that used Claude, people might not use it: https://github.com/flyngmt/ACGC-PC-Port
- My fork (code written by hand): https://github.com/randomouscrap98/AP_ACGC_PC
- Already posted in the discord about starting it, waiting to see how people react to the 
  origin of the fork



## Randomizer

### Goals (any/all/configurable):
- Paying off house (various levels)
- Bug/fish journal registrations percentages
- Certain number of villager portraits
- Certain amount of nook catalog filled

### Locations:
- Bug/fish journal registrations
- getting to certain friendship levels with villagers (up to n)
- nook catalog registration

### Items:
- Filler (bells, decorations, clothes)
- Months
- Time of day
- Weather
- Holidays (important ones anyway)
- Tom nook store upgrades
- Tools (nook will not sell them until you unlock them, or perhaps
  never sells them and you just receive them in the mail)



## QOL plans:

### Pending:
- Skip the Rover intro (supply name and city)
  - Name is 5 chars
  - City is ? chars
- Skip the Tom Nook tutorial
  - Might have to pre-select a house?
- Friendship multiplier (to make it go faster)
- Cost multiplier (to make things easier to buy)
- Increase the chance villagers give you new items for helping them, maybe up to 100%.
- Starting town fruit (random)

### Implemented



## Issues:

### Pending:
- Saving and quitting sends you back to the title menu, but then start game takes you
  back as though you never saved
- Weather seems randomized at startup, is this normal? Or did it just happen to rain 
  between two points?

### Resolved
- Required msys, I'm not a Windows user. Replaced with Podman, which should also work on Windows?
- Had weird issues with float imprecision on default build:
  - Character randomly flipped
  - Acre transitions had a wonky camera that went close to the ground
- ONE import was bad due to windows paths:
  - src/static/jaudio_NES/internal/ja_calc.c
