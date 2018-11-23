#ifndef _REPL_ENV_H_
#define _REPL_ENV_H_

#include "env.h"

void repl_env_populate(MalEnv *);
void repl_env_argv(MalEnv *, int, char **);

#endif
