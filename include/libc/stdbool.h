#ifndef _STDBOOL_H_
#define _STDBOOL_H_

// GC (MWCC) stand-in: MSL has no stdbool.h. PC builds never see include/libc.
#ifndef __cplusplus
#define bool unsigned char
#define true 1
#define false 0
#endif
#define __bool_true_false_are_defined 1

#endif
