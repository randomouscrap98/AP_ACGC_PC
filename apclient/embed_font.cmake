# Writes a binary file (the text client's font, lib/fonts/) as a C byte array, so the DLL needs no font file.
# Usage: cmake -DIN=<file> -DOUT=<header> -P embed_font.cmake (apclient/CMakeLists.txt)
file(READ "${IN}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
file(WRITE "${OUT}.tmp" "// Generated from lib/fonts/ by embed_font.cmake, don't edit\n"
    "static const unsigned char AP_UI_FONT[] = {${bytes}};\n")
# Only touch the header when it changed (no needless rebuilds)
file(COPY_FILE "${OUT}.tmp" "${OUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUT}.tmp")
