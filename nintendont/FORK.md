# Animal Crossing AP fork of Nintendont

Modified version of Nintendont (https://github.com/FIX94/Nintendont), copied from commit
`7a183459939653cb7bcf59ab137c2758a9f9fae0` (2026-11-18, v6.510). Licensed GPL version 2 (`COPYING`), separately from
the rest of this repository. It is a separate program: the game and the AP client only talk to it over the network and
through game RAM.

Build: `./build_podman.sh --nintendont` at the repository root (`Dockerfile.nintendont` pins the old devkitPro
toolchain Nintendont needs). Output: `build/nintendont/apps/AnimalCrossingAP`.
