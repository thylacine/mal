#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "console_input.h"
#include "e.h"
#include "types.h"
#include "env.h"
#include "mal.h"
#include "printer.h"
#include "reader.h"
#include "repl_env.h"

#define VERSION_STR "0.0.0"

static const char * const id_ = "v" VERSION_STR;

struct mal_options mal_options = {
	.verbosity = 0,
};

static MalData *eval_ast(MalData *, MalEnv *);

MalData * mal_read(const char *prompt);
MalData * mal_print(MalData *s);
MalData * mal_rep(MalEnv *env);

#define USAGE_FULL (1<<0)
static
void
usage_(const char *prog, unsigned int flags)
{
	FILE *f = (flags & USAGE_FULL) ? stdout : stderr;
	char *x = strrchr(prog, '/');

	if (x && *(x + 1)) {
		prog = x + 1;
	}

	if (flags & USAGE_FULL) {
		fprintf(f, "%s -- some sort of list processor\n\n",
		        prog);
	}

	fprintf(f, "Usage: %s [-v]\n", prog);

	if (flags & USAGE_FULL) {
		fprintf(f, "\nOptions:\n"
		           "\t-h -- help\n"
		           "\t-v -- increase verbosity\n");

		fprintf(f, "\n%*s\n", 78, id_);
	}
	fflush(f);
}

MalData *
mal_read(const char *prompt)
{
	MalData *md;
	char *line;

	line = console_input(prompt);
	if (line == NULL) {
		E_THROW(E_EOF, NULL);
	}
	console_input_history_add(line);

	md = read_str(line);
	free(line);

	return md;
}

MalData *
mal_eval(MalData *ast, MalEnv *env)
{
tail_call:

	assert(env != NULL);

	if (ast && ast->type == MAL_LIST) {
		MalData *md, *first;

		if (((MalList *)ast)->malmulti.len == 0) {
			return ast;
		}

		/* special forms */
		first = ((MalList *)ast)->malmulti.multi[0];
		assert(first != NULL);

		if (first->type == MAL_SYMBOL) {
			if (strcmp(((MalSymbol *)first)->symbol, "def!") == 0) {
				MalSymbol *key;

				if (((MalMulti *)ast)->len != 3) {
					LOG_ERROR("%s expected %zu argument%s, got %zu", "def!", 2, "s", ((MalMulti *)ast)->len);
					E_THROW(E_PARSE_FAILURE, NULL);
				}

				key = (MalSymbol *)((MalMulti *)ast)->multi[1];
				if (((MalData *)key)->type != MAL_SYMBOL) {
					LOG_ERROR("%s argument %zu must be %s", "def!", 1, "symbol");
					E_THROW(E_TYPE, key);
				}

				md = mal_eval(((MalMulti *)ast)->multi[2], env);

				MalEnv_set(env, key, md);

				return md;

			} else if (strcmp(((MalSymbol *)first)->symbol, "do") == 0) {
				size_t i;
				md = NULL;
				for (i = 1; i + 1 < ((MalMulti *)ast)->len; i++) {
					md = mal_eval(((MalMulti *)ast)->multi[i], env);
				}
				if (i < ((MalMulti *)ast)->len) {
					ast = ((MalMulti *)ast)->multi[i];
					goto tail_call;
				}
				return md;

			} else if (strcmp(((MalSymbol *)first)->symbol, "fn*") == 0) {
				if (((MalMulti *)ast)->len != 3) {
					LOG_ERROR("%s expected %zu argument%s, got %zu", "fn*", 2, "s", ((MalMulti *)ast)->len);
					E_THROW(E_PARSE_FAILURE, NULL);
				}

				md = MalData_new(MAL_CLOSURE);

				((MalClosure *)md)->env = env;
				((MalClosure *)md)->params = ((MalMulti *)ast)->multi[1];
				((MalClosure *)md)->body = ((MalMulti *)ast)->multi[2];

				return md;

			} else if (strcmp(((MalSymbol *)first)->symbol, "if") == 0) {
				MalData *mal_nil = MalData_new(MAL_NIL);
				MalData *mal_true = MalData_new(MAL_TRUE);
				size_t i;

				if (((MalMulti *)ast)->len <= 1) {
					LOG_ERROR("%s expected at least %zu argument%s, got %zu", "if", 1, "", ((MalMulti *)ast)->len);
					E_THROW(E_PARSE_FAILURE, NULL);
				}

				if (MalData_compare(mal_eval((MalData *)((MalMulti *)ast)->multi[1], env), mal_true) == 0) {
					i = 2;
				} else {
					i = 3;
				}

				if (((MalMulti *)ast)->len > i) {
					ast = ((MalMulti *)ast)->multi[i];
					goto tail_call;
				} else {
					md = mal_nil;
				}

				return md;

			} else if (strcmp(((MalSymbol *)first)->symbol, "let*") == 0) {
				MalEnv *newenv;
				MalMulti *lenv;
				size_t i;

				if (((MalMulti *)ast)->len != 3) {
					LOG_ERROR("%s expected %zu argument%s, got %zu", "let*", 2, "s", ((MalMulti *)ast)->len);
					E_THROW(E_PARSE_FAILURE, NULL);
				}

				lenv = (MalMulti *)((MalMulti *)ast)->multi[1];
				if ( ! MalData_isSequential((MalData *)lenv)) {
					LOG_ERROR("%s argument %zu must be %s", "let*", 1, "sequential");
					E_THROW(E_PARSE_FAILURE, NULL);
				}

				newenv = MalEnv_new(env, NULL, NULL, 0);

				if (lenv->len % 2) {
					LOG_ERROR("%s expected paired arguments, got %zu", "let*", lenv->len);
					E_THROW(E_PARSE_FAILURE, NULL);
				}
				for (i = 0; i < lenv->len; i += 2) {
					MalSymbol *key;
					MalData *value;

					key = (MalSymbol *)lenv->multi[i];
					if (key->maldata.type != MAL_SYMBOL) {
						LOG_ERROR("%s argument %zu must be %s", "let*", i + 1, "symbol");
						E_THROW(E_PARSE_FAILURE, key);
					}
					value = mal_eval(lenv->multi[i + 1], newenv);
					MalEnv_set(newenv, key, value);
				}

				env = newenv;
				ast = ((MalMulti *)ast)->multi[2];
				goto tail_call;
			}
		}

		md = eval_ast(ast, env);
		if (md == NULL) {
			E_THROW(E_RESOURCE, NULL);
		}
		assert(md->type == MAL_LIST);
		assert(((MalList *)md)->malmulti.len > 0);

		first = ((MalList *)md)->malmulti.multi[0];

		switch (first->type) {
		case MAL_FUNCTION:
			return ((MalFunction *)first)->fn(((MalMulti *)md)->len - 1, ((MalMulti *)md)->multi + 1, env);

		case MAL_CLOSURE:
		{
			MalClosure *mc = (MalClosure *)first;
			MalEnv *fn_env;
			MalData **values = ((MalMulti *)md)->multi + 1;
			size_t values_len = ((MalMulti *)md)->len - 1;

			fn_env = MalEnv_new(mc->env, mc->params, values, values_len);
			fn_env->args = 1;

			env = fn_env;
			ast = mc->body;
			goto tail_call;
		}

		default:
			E_THROW(E_PARSE_FAILURE, first);
		}
	}

	return eval_ast(ast, env);
}


static
MalData *
eval_ast(MalData *ast, MalEnv *env)
{
	MalData *r = NULL;

	assert(env != NULL);

	if (ast == NULL) {
		return NULL;
	}

	switch (ast->type) {
	case MAL_SYMBOL:
		r = MalEnv_get(env, (MalSymbol *)ast);
		if (r == NULL) {
			E_THROW(E_UNDEFINED_SYMBOL, ast);
		}
		break;

	case MAL_MULTI:
	case MAL_LIST:
	case MAL_VECTOR:
		do {
			size_t i;
			MalMulti *oldl = (MalMulti *)ast;

			r = MalData_new(ast->type);

			for (i = 0; i < oldl->len; i++) {
				MalData *md;

				md = mal_eval(oldl->multi[i], env);
				if (md == NULL) {
					E_THROW(E_RESOURCE, NULL);
				}
				MalMulti_add((MalMulti *)r, md);
			}
		} while (0);
		break;

	case MAL_HASHMAP:
		do {
			size_t i;
			MalHashmap *oldhm = (MalHashmap *)ast;

			r = MalData_new(ast->type);

			for (i = 0; i < oldhm->dict.len; i++) {
				MalData *key, *value;

				key = mal_eval(oldhm->dict.entries[i].key, env);
				value = mal_eval(oldhm->dict.entries[i].value, env);
				if (key == NULL || value == NULL) {
					E_THROW(E_RESOURCE, NULL);
				}
				MalHashmap_set_more((MalHashmap *)r, key, value);
			}
			MalHashmap_set_done((MalHashmap *)r);
		} while (0);
		break;

	default:
		r = ast;
	}

	return r;
}

MalData *
mal_print(MalData *s)
{
	unsigned int flags = PR_STR_READABLY;

	if (mal_options.verbosity) {
		flags |= PR_STR_DEBUG;
	}

	return pr_str(s, flags);
}

MalData *
mal_rep(MalEnv *env)
{
	const char *prompt = "user> ";

	return mal_print(mal_eval(mal_read(prompt), env));
}

int
main(int argc, char *argv[])
{
	MalEnv *repl_env;
	int c;

	GC_INIT();

	while ( (c = getopt(argc, argv, "hv")) != EOF) {
		switch (c) {
		case 'h':
			usage_(argv[0], 1);
			exit(EX_OK);

		case 'v':
			mal_options.verbosity += 1;
			break;

		default:
			usage_(argv[0], 0);
			exit(EX_USAGE);
		}
	}

	logger_init_default(L_ERROR + mal_options.verbosity);

	console_input_init(argv[0]);

	E_START
	case E_TRY:
		repl_env = MalEnv_new(NULL, NULL, NULL, 0);
		repl_env_populate(repl_env);
		break;

	case E_RESOURCE:
		exit(EX_OSERR);

	E_DONE

	reader_init();

	mal_eval(read_str("(def! not (fn* (a) (if a false true)))"), repl_env);

	for (;;) {
		MalData *x = NULL;
		int eof = 0;

		E_START
		case E_TRY:
			x = mal_rep(repl_env);
			break;

		case E_EOF:
			eof = 1;
			x = NULL;
			break;

		case E_PARSE_FAILURE:
			fprintf(stderr, "invalid input\n");
			x = NULL;
			break;

		case E_UNDEFINED_SYMBOL:
			fprintf(stderr, "undefined symbol\n");
			md_dumpf(stderr, E_DATA);
			x = NULL;
			break;

		case E_TYPE:
			fprintf(stderr, "type error\n");
			md_dumpf(stderr, E_DATA);
			x = NULL;
			break;

		case E_RESOURCE:
			fprintf(stderr, "resources exhausted\n");
			md_dumpf(stderr, E_DATA);
			x = NULL;
			break;

		E_DONE

		if (x || eof) {
			fputc('\n', stdout);
		}

		if (eof) {
			break;
		}

		GC_gcollect();
	}

	puts("goodbye");

	console_input_fini();

	exit(EX_OK);
}
