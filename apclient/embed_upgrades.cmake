# Wraps the apworld's slot_data_upgrades.json in a C++ raw string, so the DLL applies the same steps as the apworld.
# Usage: cmake -DIN=<json> -DOUT=<header> -P embed_upgrades.cmake (apclient/CMakeLists.txt, ignore/tests/run.sh)
file(READ "${IN}" json)
file(WRITE "${OUT}.tmp" "// Generated from slot_data_upgrades.json by embed_upgrades.cmake, don't edit\n"
    "static const char AP_SLOTDATA_UPGRADES_JSON[] = R\"json(${json})json\";\n")
# Only touch the header when it changed (no needless rebuilds)
file(COPY_FILE "${OUT}.tmp" "${OUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUT}.tmp")
