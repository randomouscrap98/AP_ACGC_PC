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

### Ideas:
- Items sent by mail (tools, which are priority, etc)
- Nooksanity: can buy archipelago items in nook's shop? maybe could have
  the shop sell specific things in a pool in specific months? maybe 
  could unlock specific weekdays? kind of confusing but would give you
  7 days times 12 months worth of items, but with nook's shop changing
  how many items it sells, this might not work

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
- Villager whitelist/blacklist
  - plus blacklist/whitelist by personality
- Starting town grass shape? is that a thing in this version?

### Implemented



## Issues:

### Pending:
- Weather seems randomized at startup, is this normal? Or did it just happen to rain 
  between two points?

### Resolved
- Required msys, I'm not a Windows user. Replaced with Podman, which should also work on Windows?
- Had weird issues with float imprecision on default build:
  - Character randomly flipped
  - Acre transitions had a wonky camera that went close to the ground
- ONE import was bad due to windows paths:
  - src/static/jaudio_NES/internal/ja_calc.c
- Saving and quitting sends you back to the title menu, but then start game takes you
  back as though you never saved
  - I'm not quite sure the way I solved it is any good; apparently this whole system was
    tacked on by the pc port and eeeghhhh we'll see I guess.
- Game uses custom allocator, breaks some of the libraries apclientpp requires
  - I tried the most basic apclient initialization I could, and the game crashed before
    it even got to main? Some global allocations failed because, I THINK, the allocator
    is overridden for the game and the allocator is not setup until the game sets it up
    at the start of main.
  - Had to move ALL the apclient stuff into its own dll so it can use normal allocators
    and whatever, while the actual game itself can continue using its own fancy crap
- apclient complains that although zlib is not required now, it WILL be required
  in the future. I don't know how truthful that is but might as well...


## Build
- Changed to use podman instead of requiring a specific setup on your pc
  - Podman is a generally easy install on most systems...: https://podman.io/docs/installation
- Build goes to ./pc/build32, which is where the original put it. I'm not a fan but I don't 
  feel like changing it right now.
- I'm using a clang lsp, there's some issues:
  - Needed to produce compile_commands.json, but we're using a container, so it's a bit complicated
    - Bunch of sed crap to edit the compile commands to not point at /build
    - also have to move some of the build files into a ".host" version of the file before editing
      so the container isn't rebuilt every single time due to edits
  - containerized build means a lot of the stuff you might need for an lsp is not on your system.
    the build scripts take care of this: build once, and it copies all the headers out of the
    container into an ignore folder and sets up a .clangd for you (gitignored) specific to your 
    system
  - lib/gcc/*/include is excluded from the container copy because it interferes with system
    headers clangd uses (got a _mm_prefetch multiple define error)

### Additional info
- Followed the instructions at https://github.com/black-sliver/apclientpp:
  - Vendored a bunch of crap: asio, cacert, json, valijson, websocketpp, wswrap (and apclientpp itself)
    - additional vendor for myself: ini.h and zlib. zlib not necessarily required for apclientpp now,
      but will be in the future?
  - Required VERY specific versions of some of these:
    - websocketpp uses stuff from asio removed in newer versions, pinned 1.30.2
    - OpenSSL pinned to 3.5.8 because 4 is a large change
    - SDL2 pinned to 2.30.10 because the pc port indicated it wanted that (not sure if that's required)


