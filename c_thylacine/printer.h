#ifndef _PRINTER_H_
#define _PRINTER_H_

#include "types.h"

#define PR_STR_READABLY (1<<0)
#define PR_STR_DEBUG (1<<1)
MalData * pr_str(MalData *, unsigned int);
int pr_str_buf(char **buf, size_t *buf_alloc, size_t *buf_used, MalData *, unsigned int);

#endif
