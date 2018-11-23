#ifndef _MAL_H_
#define _MAL_H_

#include "types.h"
#include "env.h"

struct mal_options {
	int verbosity;
};

extern struct mal_options mal_options;

MalData * mal_eval(MalData *ast, MalEnv *env);

#endif /* _MAL_H_ */
