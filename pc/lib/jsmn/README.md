Vendored from https://github.com/zserge/jsmn, tag `v1.1.0` (release tarball, `jsmn.h` unmodified:
sha256 1ed6154dedf009212a08a397e9c4ed50a0ce31d5a8301bb294e137ae3188c13b). Licence: MIT, see `LICENSE`
(copied to `bin/licenses/jsmn-LICENSE.txt` by the build, like glad/stb).

Single header, used by `pc/src/pc_ap_slotdata_parse.c` (the C slot_data parser, built with `JSMN_STATIC` +
`JSMN_STRICT`): tokenizes the slot_data JSON into a static token array, the parse logic in that file does the rest.
