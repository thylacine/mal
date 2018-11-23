#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "env.h"
#include "types.h"
#include "e.h"


MalEnv *
MalEnv_new(MalEnv *outer, MalData *binds, MalData **exprs, size_t expr_len)
{
	MalEnv *env;

	env = GC_MALLOC(sizeof *env);
	if (env == NULL) {
		LOG_ERROR("%s:%s", "malloc", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}

	memset(env, 0, sizeof *env);
	env->outer = outer;

	LOG_TRACE("MalEnv_new %p outer:%p", env, env->outer);

	if (binds != NULL) {
		MalData *bind_sym;
		MalData *bind_val;

		if (MalData_isSequential(binds)) {
			size_t i;
			for (i = 0; i < ((MalMulti *)binds)->len; i++) {
				bind_sym = ((MalMulti *)binds)->multi[i];
				if (i < expr_len) {
					bind_val = exprs[i];
				} else {
					bind_val = MalData_new(MAL_NIL);
				}

				if (bind_sym->type != MAL_SYMBOL) {
					LOG_ERROR("environment bindings must be symbols");
					E_THROW(E_TYPE, bind_sym);
				}

				if (strcmp(((MalSymbol *)bind_sym)->symbol, "&") == 0) {
					if ((i + 1) >= ((MalMulti *)binds)->len) {
						LOG_ERROR("& without symbol");
						E_THROW(E_PARSE_FAILURE, NULL);
					}
					bind_sym = ((MalMulti *)binds)->multi[i + 1];
					if (bind_sym->type != MAL_SYMBOL) {
						LOG_ERROR("environment bindings must be symbols");
						E_THROW(E_TYPE, bind_sym);
					}

					bind_val = MalData_new(MAL_LIST);

					for (/**/; i < expr_len; i++) {
						MalMulti_add((MalMulti *)bind_val, exprs[i]);
					}

					i++;
				}
				mal_dictionary_set(&env->dict, bind_sym, bind_val, MAL_DICTIONARY_SET_NOSORT);
			}
			mal_dictionary_sort(&env->dict);
		} else if (binds->type == MAL_SYMBOL && expr_len >= 1) {
			bind_sym = binds;
			bind_val = exprs[0];
			mal_dictionary_set(&env->dict, bind_sym, bind_val, 0);
		} else {
			LOG_ERROR("environment bindings must be symbols");
			E_THROW(E_TYPE, binds);
		}
	}

	return env;
}

MalEnv *
MalEnv_set(MalEnv *env, MalSymbol *key, MalData *value)
{
	MalData *oldvalue;

	assert(env != NULL);
	assert(key != NULL);
	assert(value != NULL);

	oldvalue = mal_dictionary_set(&env->dict, (MalData *)key, value, 0);

	LOG_TRACE("%s '%s' as %s in %p", oldvalue ? "replaced" : "set", key->symbol, MalData_typeName(value), (void *)env);

	return env;
}

MalEnv *
MalEnv_find(MalEnv *env, MalSymbol *key, MalData **valuep)
{
	assert(env != NULL);
	assert(key != NULL);
	assert(valuep != NULL);

tail_call:
	*valuep = mal_dictionary_get(&env->dict, (MalData *)key);
	if (*valuep != NULL) {
		return env;
	}

	if (env->outer == NULL) {
		return NULL;
	}

	env = env->outer;
	goto tail_call;
}

MalData *
MalEnv_get(MalEnv *env, MalSymbol *key)
{
	MalData *value;

	assert(env != NULL);
	assert(key != NULL);

	env = MalEnv_find(env, key, &value);
	if (env) {
		return value;
	}
	return NULL;
}

void
MalEnv_dumpf(FILE *f, MalEnv *env) {
	size_t i;
	int l;

	assert(env != NULL);

	f = f ? f : stderr;

	l = fprintf(f, "-- env:%p outer:%p alloc:%zu len:%zu --\n",
	            (void *)env, (void *)env->outer, env->dict.alloc, env->dict.len);

	for (i = 0; i < env->dict.len; i++) {
		fprintf(f, "  k:%zu ", i);
		md_dumpf(f, (MalData *)env->dict.entries[i].key);
		fprintf(f, "  v:%zu ", i);
		md_dumpf(f, (MalData *)env->dict.entries[i].value);
	}

	for (l--; l > 0; l--) {
		putchar('-');
	}
	putchar('\n');
}
