#ifndef PC_DIRS_H
#define PC_DIRS_H

#include <stdlib.h>
#include "pc_types.h"

#define PC_PATHSIZE   1024

#ifdef __cplusplus
extern "C" {
#endif

int pc_card_dir_set_root(const char * dir);
void pc_card_dir_out(s32 chan, char * out, size_t size);
void pc_card_file_out(s32 chan, char * out, size_t size, const char * fname);
void pc_card_dir_create(void);

#ifdef __cplusplus
}
#endif

#endif
