#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "e.h"
#include "types.h"
#include "env.h"
#include "core.h"
#include "repl_env.h"

void
repl_env_populate(MalEnv *env)
{
	struct core_ns_entry *entry;

	for (entry = core_ns; entry->symbol; entry++) {
		MalEnv_set(env, entry->symbol, entry->value);
	}
}

void
repl_env_argv(MalEnv *env, int argc, char **argv)
{
	MalMulti *av_list;
	int i;

	av_list = (MalMulti *)MalData_new(MAL_LIST);

	for (i = 0; i < argc; i++) {
		MalMulti_add(av_list, (MalData *)MalString_new(argv[i]));
	}

	MalEnv_set(env, MalSymbol_new("*ARGV*"), (MalData *)av_list);
}
