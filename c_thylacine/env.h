#ifndef _ENV_H_
#define _ENV_H_

#include <stdio.h>

#include "types.h"

typedef struct MalEnv {
	struct MalEnv *outer;
	struct mal_dictionary dict;
	unsigned int args : 1;
} MalEnv;

MalEnv * MalEnv_new(MalEnv *, MalData *, MalData **, size_t);
MalEnv * MalEnv_set(MalEnv *, MalSymbol *, MalData *);
MalEnv * MalEnv_find(MalEnv *, MalSymbol *, MalData **);
MalData * MalEnv_get(MalEnv *, MalSymbol *);
void MalEnv_dumpf(FILE *, MalEnv *);

#endif
