// pc_debug.c - --debug state (see pc_debug.h)
#include "pc_debug.h"

PcDebug g_pc_debug;

FILE* pc_debug_open(const char* name) {
    char path[256];

    snprintf(path, sizeof(path), "logs/%s", name);
    return fopen(path, "w");
}
