#ifndef _CORE_H_
#define _CORE_H_

#include "types.h"

struct core_ns_entry {
	MalSymbol *symbol;
	MalData *value;
};

extern
struct core_ns_entry core_ns[];

#endif
