#ifndef _READER_H_
#define _READER_H_

#include "types.h"

void reader_init(void);
void reader_fini(void);
MalData * read_str(const char *);

#endif
