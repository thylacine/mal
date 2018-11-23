#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "e.h"
#include "core.h"
#include "env.h"
#include "mal.h"
#include "printer.h"
#include "reader.h"
#include "strbuf.h"
#include "console_input.h"

#define CORE_NS_ENTRY_SYMBOL(__sym__) { .maldata = { .type = MAL_SYMBOL }, .symbol = #__sym__ }
#define CORE_NS_ENTRY_FUNCTION(__fn__) { .maldata = { .type = MAL_FUNCTION }, .fn = __fn__ }
#define CORE_NS_ENTRY(__sym__, __name__) { .symbol = & __name__ ## sym_, .value = (MalData *)& __name__ ## fn_ }
#define CORE_FN_PROTO(__sym__, __name__) \
	static MalData * __name__(size_t, MalData **, MalEnv *); \
	static MalSymbol __name__ ## sym_ = CORE_NS_ENTRY_SYMBOL(__sym__); \
	static MalFunction __name__ ## fn_ = CORE_NS_ENTRY_FUNCTION(__name__)

#define FN_ARITY(__fn__, __arity__) do {\
	if (argc < (__arity__)) {\
		LOG_ERROR("%s expected %zu argument%s, got %zu", (__fn__), (__arity__), (__arity__) > 1 ? "s" : "", argc);\
		E_THROW(E_PARSE_FAILURE, NULL);\
	}\
} while (0)
#define FN_ARG_TYPE(__fn__, __n__, __type__, __what__) do {\
	if (argv[(__n__)]->type != (__type__)) {\
		LOG_ERROR("%s argument %zu must be %s", (__fn__), (__n__) + 1, (__what__));\
		E_THROW(E_TYPE, argv[(__n__)]);\
	}\
} while (0)
#define FN_ARG_FUNCTION(__fn__, __n__) do {\
	if (argv[(__n__)]->type != MAL_FUNCTION \
	&&  argv[(__n__)]->type != MAL_CLOSURE) {\
		LOG_ERROR("%s argument %zu must be %s", (__fn__), (__n__) + 1, "function");\
		E_THROW(E_TYPE, argv[(__n__)]);\
	}\
} while (0)
#define FN_ARG_SEQUENCE(__fn__, __n__) do {\
	if (argv[(__n__)]->type != MAL_LIST \
	&&  argv[(__n__)]->type != MAL_VECTOR \
	&&  argv[(__n__)]->type != MAL_MULTI) {\
		LOG_ERROR("%s argument %zu must be %s", (__fn__), (__n__) + 1, "sequence");\
		E_THROW(E_TYPE, argv[(__n__)]);\
	}\
} while (0)
#define FN_ARGS_PAIRED_AFTER(__fn__, __n__) do {\
	if ((argc - (__n__)) % 2 != 0) {\
		LOG_ERROR("%s expected remaining arguments to be paired, got %zu", (__fn__), argc - (__n__));\
		E_THROW(E_PARSE_FAILURE, NULL);\
	}\
} while (0)

#define CORE_FN_DEF(__name__) \
static \
MalData * \
__name__(size_t argc, MalData **argv, MalEnv *env)

CORE_FN_PROTO(env-keys, core_env_keys_);

CORE_FN_PROTO(+, core_plus_);
CORE_FN_PROTO(-, core_minus_);
CORE_FN_PROTO(*, core_mult_);
CORE_FN_PROTO(/, core_div_);
CORE_FN_PROTO(prn, core_prn_);
CORE_FN_PROTO(println, core_println_);
CORE_FN_PROTO(pr-str, core_pr_str_);
CORE_FN_PROTO(str, core_str_);
CORE_FN_PROTO(list, core_list_);
CORE_FN_PROTO(list?, core_islist_);
CORE_FN_PROTO(empty?, core_isempty_);
CORE_FN_PROTO(count, core_count_);
CORE_FN_PROTO(=, core_equals_);
CORE_FN_PROTO(<, core_lt_);
CORE_FN_PROTO(<=, core_lte_);
CORE_FN_PROTO(>, core_gt_);
CORE_FN_PROTO(>=, core_gte_);
CORE_FN_PROTO(read-string, core_read_string_);
CORE_FN_PROTO(slurp, core_slurp_);
CORE_FN_PROTO(atom, core_atom_);
CORE_FN_PROTO(atom?, core_isatom_);
CORE_FN_PROTO(deref, core_deref_);
CORE_FN_PROTO(reset!, core_reset_);
CORE_FN_PROTO(swap!, core_swap_);
CORE_FN_PROTO(eval, core_eval_);
CORE_FN_PROTO(cons, core_cons_);
CORE_FN_PROTO(concat, core_concat_);
CORE_FN_PROTO(time-ms, core_time_ms_);
CORE_FN_PROTO(nth, core_nth_);
CORE_FN_PROTO(first, core_first_);
CORE_FN_PROTO(rest, core_rest_);
CORE_FN_PROTO(nil?, core_isnil_);
CORE_FN_PROTO(true?, core_istrue_);
CORE_FN_PROTO(false?, core_isfalse_);
CORE_FN_PROTO(symbol?, core_issymbol_);
CORE_FN_PROTO(keyword?, core_iskeyword_);
CORE_FN_PROTO(vector?, core_isvector_);
CORE_FN_PROTO(map?, core_ismap_);
CORE_FN_PROTO(sequential?, core_issequential_);
CORE_FN_PROTO(fn?, core_isfn_);
CORE_FN_PROTO(macro?, core_ismacro_);
CORE_FN_PROTO(number?, core_isnumber_);
CORE_FN_PROTO(string?, core_isstring_);
CORE_FN_PROTO(throw, core_throw_);
CORE_FN_PROTO(apply, core_apply_);
CORE_FN_PROTO(map, core_map_);
CORE_FN_PROTO(symbol, core_symbol_);
CORE_FN_PROTO(keyword, core_keyword_);
CORE_FN_PROTO(vector, core_vector_);
CORE_FN_PROTO(hash-map, core_hashmap_);
CORE_FN_PROTO(get, core_get_);
CORE_FN_PROTO(contains?, core_contains_);
CORE_FN_PROTO(keys, core_keys_);
CORE_FN_PROTO(vals, core_vals_);
CORE_FN_PROTO(assoc, core_assoc_);
CORE_FN_PROTO(dissoc, core_dissoc_);
CORE_FN_PROTO(meta, core_meta_);
CORE_FN_PROTO(with-meta, core_with_meta_);
CORE_FN_PROTO(conj, core_conj_);
CORE_FN_PROTO(seq, core_seq_);
CORE_FN_PROTO(readline, core_readline_);


static MalSymbol core_host_language_sym_ = { .maldata = { .type = MAL_SYMBOL }, .symbol = "*host-language*" };
static MalString core_host_language_str_ = { .maldata = { .type = MAL_STRING }, .string = "c11_thylacine", .len = 13, .alloc = 0 };

struct core_ns_entry core_ns[] = {
	{ .symbol = &core_host_language_sym_, .value = (MalData *)&core_host_language_str_ },

	CORE_NS_ENTRY(env-keys, core_env_keys_),

	CORE_NS_ENTRY(+, core_plus_),
	CORE_NS_ENTRY(-, core_minus_),
	CORE_NS_ENTRY(*, core_mult_),
	CORE_NS_ENTRY(/, core_div_),
	CORE_NS_ENTRY(prn, core_prn_),
	CORE_NS_ENTRY(println, core_println_),
	CORE_NS_ENTRY(pr-str, core_pr_str_),
	CORE_NS_ENTRY(str, core_str_),
	CORE_NS_ENTRY(list, core_list_),
	CORE_NS_ENTRY(list?, core_islist_),
	CORE_NS_ENTRY(empty?, core_isempty_),
	CORE_NS_ENTRY(count, core_count_),
	CORE_NS_ENTRY(=, core_equals_),
	CORE_NS_ENTRY(<, core_lt_),
	CORE_NS_ENTRY(<=, core_lte_),
	CORE_NS_ENTRY(>, core_gt_),
	CORE_NS_ENTRY(>=, core_gte_),
	CORE_NS_ENTRY(read-string, core_read_string_),
	CORE_NS_ENTRY(slurp, core_slurp_),
	CORE_NS_ENTRY(atom, core_atom_),
	CORE_NS_ENTRY(atom?, core_isatom_),
	CORE_NS_ENTRY(deref, core_deref_),
	CORE_NS_ENTRY(reset!, core_reset_),
	CORE_NS_ENTRY(swap!, core_swap_),
	CORE_NS_ENTRY(eval, core_eval_),
	CORE_NS_ENTRY(cons, core_cons_),
	CORE_NS_ENTRY(concat, core_concat_),
	CORE_NS_ENTRY(time-ms, core_time_ms_),
	CORE_NS_ENTRY(nth, core_nth_),
	CORE_NS_ENTRY(first, core_first_),
	CORE_NS_ENTRY(rest, core_rest_),
	CORE_NS_ENTRY(nil?, core_isnil_),
	CORE_NS_ENTRY(true?, core_istrue_),
	CORE_NS_ENTRY(false?, core_isfalse_),
	CORE_NS_ENTRY(symbol?, core_issymbol_),
	CORE_NS_ENTRY(keyword?, core_iskeyword_),
	CORE_NS_ENTRY(vector?, core_isvector_),
	CORE_NS_ENTRY(map?, core_ismap_),
	CORE_NS_ENTRY(sequential?, core_issequential_),
	CORE_NS_ENTRY(fn?, core_isfn_),
	CORE_NS_ENTRY(macro?, core_ismacro_),
	CORE_NS_ENTRY(number?, core_isnumber_),
	CORE_NS_ENTRY(string?, core_isstring_),
	CORE_NS_ENTRY(throw, core_throw_),
	CORE_NS_ENTRY(apply, core_apply_),
	CORE_NS_ENTRY(map, core_map_),
	CORE_NS_ENTRY(symbol, core_symbol_),
	CORE_NS_ENTRY(keyword, core_keyword_),
	CORE_NS_ENTRY(vector, core_vector_),
	CORE_NS_ENTRY(hash-map, core_hashmap_),
	CORE_NS_ENTRY(get, core_get_),
	CORE_NS_ENTRY(contains?, core_contains_),
	CORE_NS_ENTRY(keys, core_keys_),
	CORE_NS_ENTRY(vals, core_vals_),
	CORE_NS_ENTRY(assoc, core_assoc_),
	CORE_NS_ENTRY(dissoc, core_dissoc_),
	CORE_NS_ENTRY(meta, core_meta_),
	CORE_NS_ENTRY(with-meta, core_with_meta_),
	CORE_NS_ENTRY(conj, core_conj_),
	CORE_NS_ENTRY(seq, core_seq_),
	CORE_NS_ENTRY(readline, core_readline_),

	{ .symbol = NULL, .value = NULL }
};

/* unoffical - return list of keys in current env */
CORE_FN_DEF(core_env_keys_)
{
	MalData *r;
	size_t i;

	(void)argc, (void)argv;

	LOG_TRACE("env-keys env: %p", env);
	MalEnv_dumpf(NULL, env);

	r = MalData_new(MAL_LIST);

	for (i = 0; i < env->dict.len; i++) {
		MalMulti_add((MalMulti *)r, (MalData *)(env->dict.entries[i].key));
	}

	return r;
}

CORE_FN_DEF(core_plus_)
{
	MalInteger acc_i = { .value = 0, .maldata = { .type = MAL_INTEGER } };
	MalFloat acc_f = { .value = 0.0L, .maldata = { .type = MAL_FLOAT } };
	MalType rtype = MAL_INTEGER;
	MalData *r;
	size_t i;

	(void)env;

	if (argc > 0) {
		assert(argv[0] != NULL);

		switch (argv[0]->type) {
		case MAL_INTEGER:
			acc_i.value = ((MalInteger *)argv[0])->value;
			break;
		case MAL_FLOAT:
			rtype = MAL_FLOAT;
			acc_f.value = ((MalFloat *)argv[0])->value;
			break;
		default:
			LOG_ERROR("%s argument %zu must be %s", "+", 1, "number");
			E_THROW(E_TYPE, argv[0]);
		}
	}

	for (i = 1; i < argc; i++) {
		assert(argv[i] != NULL);

		switch (argv[i]->type) {
		case MAL_INTEGER:
			switch (rtype) {
			case MAL_INTEGER:
				acc_i.value += ((MalInteger *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value += ((MalInteger *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		case MAL_FLOAT:
			switch (rtype) {
			case MAL_INTEGER:
				rtype = MAL_FLOAT;
				acc_f.value = acc_i.value;
				acc_f.value += ((MalFloat *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value += ((MalFloat *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		default:
			LOG_ERROR("%s argument %zu must be %s", "+", i + 1, "number");
			E_THROW(E_TYPE, argv[i]);
		}
	}
	r = MalData_new(rtype);

	switch (rtype) {
	case MAL_INTEGER:
		((MalInteger *)r)->value = acc_i.value;
		break;
	case MAL_FLOAT:
		((MalFloat *)r)->value = acc_f.value;
		break;
	default:
		break;
	}

	return r;
}

CORE_FN_DEF(core_minus_)
{
	MalInteger acc_i = { .value = 0, .maldata = { .type = MAL_INTEGER } };
	MalFloat acc_f = { .value = 0.0, .maldata = { .type = MAL_FLOAT } };
	MalType rtype = MAL_INTEGER;
	MalData *r;
	size_t i;

	(void)env;

	if (argc > 0) {
		assert(argv[0] != NULL);

		switch (argv[0]->type) {
		case MAL_INTEGER:
			acc_i.value = ((MalInteger *)argv[0])->value;
			break;
		case MAL_FLOAT:
			rtype = MAL_FLOAT;
			acc_f.value = ((MalFloat *)argv[0])->value;
			break;
		default:
			LOG_ERROR("%s argument %zu must be %s", "-", 1, "number");
			E_THROW(E_TYPE, argv[0]);
		}
	}

	for (i = 1; i < argc; i++) {
		assert(argv[i] != NULL);

		switch (argv[i]->type) {
		case MAL_INTEGER:
			switch (rtype) {
			case MAL_INTEGER:
				acc_i.value -= ((MalInteger *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value -= ((MalInteger *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		case MAL_FLOAT:
			switch (rtype) {
			case MAL_INTEGER:
				rtype = MAL_FLOAT;
				acc_f.value = acc_i.value;
				acc_f.value -= ((MalFloat *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value -= ((MalFloat *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		default:
			LOG_ERROR("%s argument %zu must be %s", "-", i + 1, "number");
			E_THROW(E_TYPE, argv[i]);
		}
	}

	r = MalData_new(rtype);

	switch (rtype) {
	case MAL_INTEGER:
		((MalInteger *)r)->value = acc_i.value;
		break;
	case MAL_FLOAT:
		((MalFloat *)r)->value = acc_f.value;
		break;
	default:
		break;
	}

	return r;
}

CORE_FN_DEF(core_mult_)
{
	MalInteger acc_i = { .value = 0, .maldata = { .type = MAL_INTEGER } };
	MalFloat acc_f = { .value = 0.0, .maldata = { .type = MAL_FLOAT } };
	MalType rtype = MAL_INTEGER;
	MalData *r;
	size_t i;

	(void)env;

	if (argc > 0) {
		assert(argv[0] != NULL);

		switch (argv[0]->type) {
		case MAL_INTEGER:
			acc_i.value = ((MalInteger *)argv[0])->value;
			break;
		case MAL_FLOAT:
			rtype = MAL_FLOAT;
			acc_f.value = ((MalFloat *)argv[0])->value;
			break;
		default:
			LOG_ERROR("%s argument %zu must be %s", "*", 1, "number");
			E_THROW(E_TYPE, argv[0]);
		}
	}

	for (i = 1; i < argc; i++) {
		assert(argv[i] != NULL);

		switch (argv[i]->type) {
		case MAL_INTEGER:
			switch (rtype) {
			case MAL_INTEGER:
				acc_i.value *= ((MalInteger *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value *= ((MalInteger *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		case MAL_FLOAT:
			switch (rtype) {
			case MAL_INTEGER:
				rtype = MAL_FLOAT;
				acc_f.value = acc_i.value;
				acc_f.value *= ((MalFloat *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value *= ((MalFloat *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		default:
			LOG_ERROR("%s argument %zu must be %s", "*", i + 1, "number");
			E_THROW(E_TYPE, argv[i]);
		}
	}

	r = MalData_new(rtype);

	switch (rtype) {
	case MAL_INTEGER:
		((MalInteger *)r)->value = acc_i.value;
		break;
	case MAL_FLOAT:
		((MalFloat *)r)->value = acc_f.value;
		break;
	default:
		break;
	}

	return r;
}

CORE_FN_DEF(core_div_)
{
	MalInteger acc_i = { .value = 0, .maldata = { .type = MAL_INTEGER } };
	MalFloat acc_f = { .value = 0.0, .maldata = { .type = MAL_FLOAT } };
	MalType rtype = MAL_INTEGER;
	MalData *r;
	size_t i;

	(void)env;

	if (argc > 0) {
		assert(argv[0] != NULL);

		switch (argv[0]->type) {
		case MAL_INTEGER:
			acc_i.value = ((MalInteger *)argv[0])->value;
			break;
		case MAL_FLOAT:
			rtype = MAL_FLOAT;
			acc_f.value = ((MalFloat *)argv[0])->value;
			break;
		default:
			LOG_ERROR("%s argument %zu must be %s", "/", 1, "number");
			E_THROW(E_TYPE, argv[0]);
		}
	}

	for (i = 1; i < argc; i++) {
		assert(argv[i] != NULL);

		switch (argv[i]->type) {
		case MAL_INTEGER:
			if (((MalInteger *)argv[i])->value == 0) {
				E_THROW(E_MAL, MalString_new("Division by zero"));
			}
			switch (rtype) {
			case MAL_INTEGER:
				acc_i.value /= ((MalInteger *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value /= ((MalInteger *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		case MAL_FLOAT:
			if (fabsl(((MalFloat *)argv[i])->value) < LDBL_EPSILON) {
				E_THROW(E_MAL, MalString_new("Division by zero"));
			}
			switch (rtype) {
			case MAL_INTEGER:
				rtype = MAL_FLOAT;
				acc_f.value = acc_i.value;
				acc_f.value /= ((MalFloat *)argv[i])->value;
				break;
			case MAL_FLOAT:
				acc_f.value /= ((MalFloat *)argv[i])->value;
				break;
			default:
				break;
			}
			break;

		default:
			LOG_ERROR("%s argument %zu must be %s", "/", i + 1, "number");
			E_THROW(E_TYPE, argv[i]);
		}
	}

	r = MalData_new(rtype);

	switch (rtype) {
	case MAL_INTEGER:
		((MalInteger *)r)->value = acc_i.value;
		break;
	case MAL_FLOAT:
		((MalFloat *)r)->value = acc_f.value;
		break;
	default:
		break;
	}

	return r;
}

static
void
concat_buf_(char **buf, size_t *buf_alloc, size_t *buf_used, size_t argc, MalData **argv, int pr_flags, const char *sep_str)
{
	size_t i;

	if (argc == 0) {
		*buf = GC_MALLOC_ATOMIC(1);
		if (*buf == NULL) {
			E_THROW(E_RESOURCE, NULL);
		}
		*buf[0] = '\0';
		*buf_alloc = 1;
		*buf_used = 0;
		return;
	}

	for (i = 0; i < argc; i++) {
		int l;

		assert(argv[i] != NULL);
		l = pr_str_buf(buf, buf_alloc, buf_used, argv[i], pr_flags);
		if (l < 0) {
			*buf = NULL;
			E_THROW(E_RESOURCE, NULL);
		}
		if (sep_str != NULL && *sep_str != '\0' && i != argc - 1) {
			l = strbuf_appendf(buf, buf_alloc, buf_used, sep_str);
			if (l < 0) {
				E_THROW(E_RESOURCE, NULL);
			}
		}
	}
}

CORE_FN_DEF(core_prn_)
{
	const char * const sep_str = " ";
	char *buf = NULL;
	size_t buf_alloc = 0;
	size_t buf_used = 0;
	int l;

	(void)env;

	concat_buf_(&buf, &buf_alloc, &buf_used, argc, argv, PR_STR_READABLY, sep_str);

	l = puts(buf);
	if (l == EOF) {
		LOG_ERROR("%s:%s\n", "puts", strerror(errno));
	}

	return MalData_new(MAL_NIL);
}

CORE_FN_DEF(core_println_)
{
	const char * const sep_str = " ";
	char *buf = NULL;
	size_t buf_alloc = 0;
	size_t buf_used = 0;
	int l;

	(void)env;

	concat_buf_(&buf, &buf_alloc, &buf_used, argc, argv, 0, sep_str);

	l = puts(buf);
	if (l == EOF) {
		LOG_ERROR("%s:%s\n", "puts", strerror(errno));
	}

	return MalData_new(MAL_NIL);
}

CORE_FN_DEF(core_pr_str_)
{
	const char * const sep_str = " ";
	char *buf = NULL;
	size_t buf_alloc = 0;
	size_t buf_used = 0;
	MalString *str;

	(void)env;

	concat_buf_(&buf, &buf_alloc, &buf_used, argc, argv, PR_STR_READABLY, sep_str);

	str = (MalString *)MalData_new(MAL_STRING);

	str->string = buf;
	str->alloc = buf_alloc;
	str->len = buf_used;

	return (MalData *)str;
}

CORE_FN_DEF(core_str_)
{
	const char * const sep_str = NULL;
	char * buf = NULL;
	size_t buf_alloc = 0;
	size_t buf_used = 0;
	MalString *str;

	(void)env;

	concat_buf_(&buf, &buf_alloc, &buf_used, argc, argv, 0, sep_str);

	str = (MalString *)MalData_new(MAL_STRING);

	str->string = buf;
	str->alloc = buf_alloc;
	str->len = buf_used;

	return (MalData *)str;
}

CORE_FN_DEF(core_list_)
{
	MalData *r;
	size_t i;

	(void)env;

	r = MalData_new(MAL_LIST);

	for (i = 0; i < argc; i++) {
		MalMulti_add((MalMulti *)r, argv[i]);
	}

	return r;
}

CORE_FN_DEF(core_isempty_)
{
	MalType what = MAL_TRUE;

	(void)env;

	if (argc > 0) {
		if (argv[0]->type == MAL_LIST
		||  argv[0]->type == MAL_VECTOR
		||  argv[0]->type == MAL_MULTI) {
			if (((MalMulti *)argv[0])->len) {
				what = MAL_FALSE;
			}
		} else if (argv[0]->type == MAL_HASHMAP) {
			if (((MalHashmap *)argv[0])->dict.len) {
				what = MAL_FALSE;
			}
		}
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_count_)
{
	MalData *r;

	(void)env;

	r = MalData_new(MAL_INTEGER);

	((MalInteger *)r)->value = 0;

	if (argc > 0) {
		if (argv[0]->type == MAL_LIST
		||  argv[0]->type == MAL_VECTOR
		||  argv[0]->type == MAL_MULTI) {
			((MalInteger *)r)->value = (long long)((MalMulti *)argv[0])->len;
		} else if (argv[0]->type == MAL_HASHMAP) {
			((MalInteger *)r)->value = (long long)((MalHashmap *)argv[0])->dict.len;
		} else if (argv[0]->type == MAL_STRING) {
			((MalInteger *)r)->value = (long long)((MalString *)argv[0])->len;
		}
	}

	return r;
}

CORE_FN_DEF(core_equals_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 2) {
		if (MalData_compare(argv[0], argv[1]) == 0) {
			what = MAL_TRUE;
		}
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_lt_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 2) {
		MalData *a = argv[0],
		        *b = argv[1];
		assert(a != NULL);
		assert(b != NULL);

		if ((a->type == MAL_INTEGER || a->type == MAL_FLOAT)
		&&  (b->type == MAL_INTEGER || b->type == MAL_FLOAT)) {
			if (MalData_compare(a, b) < 0) {
				what = MAL_TRUE;
			}
		}
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_lte_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 2) {
		MalData *a = argv[0],
		        *b = argv[1];
		assert(a != NULL);
		assert(b != NULL);

		if ((a->type == MAL_INTEGER || a->type == MAL_FLOAT)
		&&  (b->type == MAL_INTEGER || b->type == MAL_FLOAT)) {
			if (MalData_compare(a, b) <= 0) {
				what = MAL_TRUE;
			}
		}
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_gt_)
{
	MalType what = MAL_FALSE;
	MalData *r = NULL;

	(void)env;

	if (argc >= 2) {
		MalData *a = argv[0],
		        *b = argv[1];
		assert(a != NULL);
		assert(b != NULL);

		if ((a->type == MAL_INTEGER || a->type == MAL_FLOAT)
		&&  (b->type == MAL_INTEGER || b->type == MAL_FLOAT)) {
			if (MalData_compare(a, b) > 0) {
				what = MAL_TRUE;
			}
		}
	}

	r = MalData_new(what);

	return r;
}

CORE_FN_DEF(core_gte_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 2) {
		MalData *a = argv[0],
		        *b = argv[1];
		assert(a != NULL);
		assert(b != NULL);

		if ((a->type == MAL_INTEGER || a->type == MAL_FLOAT)
		&&  (b->type == MAL_INTEGER || b->type == MAL_FLOAT)) {
			if (MalData_compare(a, b) >= 0) {
				what = MAL_TRUE;
			}
		}
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_read_string_)
{
	(void)env;

	if (argc < 1) {
		return MalData_new(MAL_NIL);
	}

	FN_ARG_TYPE("read-string", 0, MAL_STRING, "string");

	return read_str(((MalString *)argv[0])->string);
}

CORE_FN_DEF(core_slurp_)
{
	MalString *r, *filename;
	int fd;
	struct stat st;
	off_t off = 0;

	(void)env;

	if (argc < 1) {
		return MalData_new(MAL_NIL);
	}

	FN_ARG_TYPE("slurp", 0, MAL_STRING, "string");

	filename = (MalString *)argv[0];

	fd = open(filename->string, O_RDONLY);
	if (fd < 0) {
		LOG_ERROR("%s: %s", "open", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}

	if (fstat(fd, &st) < 0) {
		LOG_ERROR("%s: %s", "fstat", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}

	r = (MalString *)MalData_new(MAL_STRING);

	r->string = GC_MALLOC_ATOMIC(st.st_size + 1);
	if (r->string == NULL) {
		E_THROW(E_RESOURCE, NULL);
	}

	while (off < st.st_size) {
		ssize_t l = read(fd, (void *)(r->string + off), st.st_size - off);
		if (l < 0) {
			if (errno == EINTR) {
				continue;
			}
			LOG_ERROR("%s: %s", "read", strerror(errno));
			close(fd);
			E_THROW(E_RESOURCE, NULL);
		}
		if (l == 0) {
			break;
		}
		off += l;
	}
	r->len = off;
	r->alloc = off + 1;
	((char *)r->string)[st.st_size] = '\0';

	close(fd);

	return (MalData *)r;
}

CORE_FN_DEF(core_atom_)
{
	MalAtom *atom;

	(void)env;

	atom = (MalAtom *)MalData_new(MAL_ATOM);

	atom->atom = argc ? argv[0] : MalData_new(MAL_NIL);

	return (MalData *)atom;
}

CORE_FN_DEF(core_deref_)
{
	(void)env;

	if (argc < 1
	||  argv[0]->type != MAL_ATOM
	||  ((MalAtom *)argv[0])->atom == NULL) {
		return MalData_new(MAL_NIL);
	}

	return ((MalAtom *)argv[0])->atom;
}

CORE_FN_DEF(core_reset_)
{
	MalData *r;

	(void)env;

	FN_ARITY("reset", 2);
	FN_ARG_TYPE("reset", 0, MAL_ATOM, "atom");


	r = argv[1];
	((MalAtom *)argv[0])->atom = r;

	return r;
}

CORE_FN_DEF(core_swap_)
{
	MalData *r;
	MalData *atom;
	MalData *fn;
	MalData *args;

	FN_ARITY("swap!", 2);

	FN_ARG_TYPE("swap!", 0, MAL_ATOM, "atom");
	atom = argv[0];

	FN_ARG_FUNCTION("swap!", 1);
	fn = argv[1];

	argc -= 2;
	argv += 2;

	args = MalData_new(MAL_LIST);
	MalMulti_add((MalMulti *)args, ((MalAtom *)atom)->atom);
	for (/**/; argc > 0 && *argv; argc--, argv++) {
		MalMulti_add((MalMulti *)args, *argv);
	}

	r = MalFunction_apply(fn, ((MalMulti*)args)->len, ((MalMulti *)args)->multi, env);

	((MalAtom *)atom)->atom = r;

	return r;
}

CORE_FN_DEF(core_eval_)
{
	if (argc < 1) {
		return MalData_new(MAL_NIL);
	}

	while (env->args && env->outer) {
		env = env->outer;
	}

	return mal_eval(argv[0], env);
}

CORE_FN_DEF(core_cons_)
{
	MalData *r;
	size_t i;

	(void)env;

	r = MalData_new(MAL_LIST);

	if (argc >= 1) {
		MalMulti_add((MalMulti *)r, argv[0]);
	}

	if (argc >= 2) {
		FN_ARG_SEQUENCE("cons", 1);

		for (i = 0; i < ((MalMulti *)argv[1])->len; i++) {
			MalMulti_add((MalMulti *)r, ((MalMulti *)argv[1])->multi[i]);
		}
	}

	return r;
}

CORE_FN_DEF(core_concat_)
{
	MalData *r;
	size_t j;

	(void)env;

	r = MalData_new(MAL_LIST);

	for (j = 0; j < argc ; j++) {
		size_t i;

		FN_ARG_SEQUENCE("concat", j);

		for (i = 0; i < ((MalMulti *)argv[j])->len; i++) {
			MalMulti_add((MalMulti *)r, ((MalMulti *)argv[j])->multi[i]);
		}
	}

	return r;
}

CORE_FN_DEF(core_time_ms_)
{
	MalData *r;
	struct timeval tv;
	long long ms;

	(void)argc, (void)argv, (void)env;

	r = MalData_new(MAL_INTEGER);

	if (gettimeofday(&tv, NULL) < 0) {
		LOG_ERROR("%s: %s", "gettimeofday", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}

	ms = tv.tv_sec * 1000LL;
	ms += tv.tv_usec / 1000LL;

	((MalInteger *)r)->value = ms;

	return r;
}

CORE_FN_DEF(core_nth_)
{
	ssize_t i;

	(void)env;

	FN_ARITY("nth", 2);
	FN_ARG_SEQUENCE("nth", 0);
	FN_ARG_TYPE("nth", 1, MAL_INTEGER, "integer");

	i = ((MalInteger *)argv[1])->value;

	if (((MalMulti *)argv[0])->len <= (size_t)i) {
		LOG_ERROR("index out of range");
#ifdef MAL_EXCEPTIONS
		E_THROW(E_MAL, MalString_new("index out of range"));
#else
		E_THROW(E_PARSE_FAILURE, NULL);
#endif /* MAL_EXCEPTIONS */
	}

	return ((MalMulti *)argv[0])->multi[i];
}

CORE_FN_DEF(core_first_)
{
	(void)env;

	FN_ARITY("first", 1);

	if (argv[0]->type == MAL_NIL) {
		return argv[0];
	}

	FN_ARG_SEQUENCE("first", 0);

	if (((MalMulti *)argv[0])->len == 0) {
		return MalData_new(MAL_NIL);
	}

	return ((MalMulti *)argv[0])->multi[0];
}

CORE_FN_DEF(core_rest_)
{
	MalData *r;
	size_t i;

	(void)env;

	FN_ARITY("rest", 1);

	if (argv[0]->type == MAL_NIL) {
		return MalData_new(MAL_LIST);
	}

	FN_ARG_SEQUENCE("rest", 0);

	r = MalData_new(MAL_LIST);

	for (i = 1; i < ((MalMulti *)argv[0])->len; i++) {
		MalMulti_add((MalMulti *)r, ((MalMulti *)argv[0])->multi[i]);
	}

	return r;
}

CORE_FN_DEF(core_issequential_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 1
	&&  (argv[0]->type == MAL_LIST
	     || argv[0]->type == MAL_VECTOR)) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_isfn_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 1
	&&  ((argv[0]->type == MAL_FUNCTION)
	     || (argv[0]->type == MAL_CLOSURE && ! ((MalClosure *)argv[0])->is_macro)) ) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_ismacro_) {
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 1
	&&  argv[0]->type == MAL_CLOSURE
	&&  ((MalClosure *)argv[0])->is_macro) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_isnumber_)
{
	MalType what = MAL_FALSE;

	(void)env;

	if (argc >= 1
	&&  (argv[0]->type == MAL_INTEGER
	     || argv[0]->type == MAL_FLOAT)) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

static
inline
MalData *
type_predicate_(size_t argc, MalData **argv, MalType type)
{
	MalType what = MAL_FALSE;

	if (argc >= 1
	&&  argv[0]->type == type) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_isstring_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_STRING);
}

CORE_FN_DEF(core_islist_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_LIST);
}

CORE_FN_DEF(core_isatom_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_ATOM);
}

CORE_FN_DEF(core_isnil_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_NIL);
}

CORE_FN_DEF(core_istrue_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_TRUE);
}

CORE_FN_DEF(core_isfalse_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_FALSE);
}

CORE_FN_DEF(core_issymbol_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_SYMBOL);
}

CORE_FN_DEF(core_iskeyword_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_KEYWORD);
}

CORE_FN_DEF(core_isvector_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_VECTOR);
}

CORE_FN_DEF(core_ismap_)
{
	(void)env;
	return type_predicate_(argc, argv, MAL_HASHMAP);
}

CORE_FN_DEF(core_throw_)
{
	MalData *e;

	(void)env;

	if (argc < 1) {
		e = MalData_new(MAL_NIL);
	} else {
		e = argv[0];
	}

	E_THROW(E_MAL, e);

	return e;
}

CORE_FN_DEF(core_apply_)
{
	MalMulti *ast;
	MalMulti *last;
	size_t i;

	FN_ARITY("apply", 2);
	FN_ARG_FUNCTION("apply", 0);
	FN_ARG_SEQUENCE("apply", argc - 1);

	last = (MalMulti *)argv[argc - 1];

	ast = (MalMulti *)MalData_new(MAL_LIST);

	for (i = 1; i < argc - 1; i++) {
		MalMulti_add(ast, argv[i]);
	}

	for (i = 0; i < last->len; i++) {
		MalMulti_add(ast, last->multi[i]);
	}

	return MalFunction_apply(argv[0], ((MalMulti *)ast)->len, ((MalMulti *)ast)->multi, env);
}

CORE_FN_DEF(core_map_)
{
	MalData *r;
	size_t i;

	FN_ARITY("map", 2);
	FN_ARG_FUNCTION("map", 0);
	FN_ARG_SEQUENCE("map", 1);

	r = MalData_new(MAL_LIST);

	for (i = 0; i < ((MalMulti *)argv[1])->len; i++) {
		MalData *md;
		MalMulti *ast = (MalMulti *)MalData_new(MAL_LIST);

		MalMulti_add(ast, ((MalMulti *)argv[1])->multi[i]);

		md = MalFunction_apply(argv[0], ast->len, ast->multi, env);

		MalMulti_add((MalMulti *)r, md);
	}

	return r;
}

CORE_FN_DEF(core_symbol_)
{
	MalData *r;

	(void)env;

	FN_ARITY("symbol", 1);
	FN_ARG_TYPE("symbol", 0, MAL_STRING, "string");

	r = MalData_new(MAL_SYMBOL);
	((MalSymbol *)r)->symbol = GC_STRDUP(((MalString *)argv[0])->string);

	return r;
}

CORE_FN_DEF(core_keyword_)
{
	MalData *r;
	const char *str;
	char *kw;

	(void)env;

	FN_ARITY("keyword", 1);

	if (argv[0]->type == MAL_KEYWORD) {
		return argv[0];
	}

	FN_ARG_TYPE("keyword", 0, MAL_STRING, "string");

	str = ((MalString *)argv[0])->string;

	r = MalData_new(MAL_KEYWORD);

	if (str[0] == ':') {
		kw = GC_STRDUP(str);
	} else {
		kw = GC_MALLOC_ATOMIC(1 + 1 + strlen(str));
		kw[0] = ':';
		strcpy(kw + 1, str);
	}
	((MalKeyword *)r)->keyword = kw;

	return r;
}


CORE_FN_DEF(core_vector_)
{
	MalData *r;
	size_t i;

	(void)env;

	r = MalData_new(MAL_VECTOR);

	for (i = 0; i < argc; i++) {
		MalMulti_add((MalMulti *)r, argv[i]);
	}

	return r;
}

CORE_FN_DEF(core_hashmap_)
{
	MalData *r;
	size_t i;

	(void)env;

	FN_ARGS_PAIRED_AFTER("hash-map", 0);

	r = MalData_new(MAL_HASHMAP);

	for (i = 0; i < argc; i += 2) {
		MalHashmap_set_more((MalHashmap *)r, argv[i], argv[i + 1]);
	}
	MalHashmap_set_done((MalHashmap *)r);

	return r;
}

CORE_FN_DEF(core_get_)
{
	MalData *r;

	(void)env;

	FN_ARITY("get", 2);

	if (argv[0]->type != MAL_HASHMAP) {
		return MalData_new(MAL_NIL);
	}

	r = MalHashmap_get((MalHashmap *)argv[0], argv[1]);
	if (r == NULL) {
		r = MalData_new(MAL_NIL);
	}

	return r;
}

CORE_FN_DEF(core_contains_)
{
	MalType what = MAL_FALSE;

	(void)env;

	FN_ARITY("contains", 2);

	if (argv[0]->type != MAL_HASHMAP) {
		return MalData_new(MAL_NIL);
	}

	if (MalHashmap_get((MalHashmap *)argv[0], argv[1])) {
		what = MAL_TRUE;
	}

	return MalData_new(what);
}

CORE_FN_DEF(core_keys_)
{
	(void)env;

	FN_ARITY("keys", 1);
	FN_ARG_TYPE("keys", 0, MAL_HASHMAP, "hash-map");

	return (MalData *)MalHashmap_keys((MalHashmap *)argv[0]);
}

CORE_FN_DEF(core_vals_)
{
	(void)env;

	FN_ARITY("vals", 1);
	FN_ARG_TYPE("vals", 0, MAL_HASHMAP, "hash-map");

	return (MalData *)MalHashmap_vals((MalHashmap *)argv[0]);
}

CORE_FN_DEF(core_assoc_)
{
	MalHashmap *r, *oldhm;
	size_t i;

	(void)env;

	FN_ARITY("assoc", 1);
	FN_ARG_TYPE("assoc", 0, MAL_HASHMAP, "hash-map");

	oldhm = (MalHashmap *)argv[0];

	FN_ARGS_PAIRED_AFTER("assoc", 1);

	r = (MalHashmap *)MalData_new(MAL_HASHMAP);

	for (i = 0; i < oldhm->dict.len; i++) {
		MalData *sym = oldhm->dict.entries[i].key,
		        *val = oldhm->dict.entries[i].value;
		MalHashmap_set_more(r, sym, val);
	}

	for (i = 1; i < argc; i += 2) {
		MalHashmap_set_more(r, argv[i], argv[i + 1]);
	}

	MalHashmap_set_done(r);

	return (MalData *)r;
}

CORE_FN_DEF(core_dissoc_)
{
	MalHashmap *r, *oldhm;
	size_t i, j;

	(void)env;

	FN_ARITY("dissoc", 1);
	FN_ARG_TYPE("dissoc", 0, MAL_HASHMAP, "hash-map");

	oldhm = (MalHashmap *)argv[0];

	r = (MalHashmap *)MalData_new(MAL_HASHMAP);

	for (i = 0; i < oldhm->dict.len; i++) {
		MalData *key = oldhm->dict.entries[i].key,
		        *val = oldhm->dict.entries[i].value;
		int elide = 0;
		for (j = 1; j < argc; j++) {
			if (MalData_compare(key, argv[j]) == 0) {
				elide = 1;
				break;
			}
		}
		if (elide) {
			continue;
		}
		MalHashmap_set_more(r, key, val);
	}

	MalHashmap_set_done(r);

	return (MalData *)r;
}

CORE_FN_DEF(core_meta_)
{
	MalData *r = NULL;
	MalData *md;

	(void)env;

	FN_ARITY("meta", 1);

	md = argv[0];
	r = md->meta;

	if (r == NULL) {
		r = MalData_new(MAL_NIL);
	}

	return r;
}

CORE_FN_DEF(core_with_meta_)
{
	MalData *r = NULL;
	MalData *md;

	(void)env;

	FN_ARITY("with-meta", 2);

	md = argv[0];

	r = MalData_clone(md);
	r->meta = argv[1];

	return r;
}

CORE_FN_DEF(core_conj_)
{
	MalData *r = NULL;
	MalData *md;
	size_t i;

	(void)env;

	FN_ARITY("conj", 2);
	FN_ARG_SEQUENCE("conj", 0);

	md = argv[0];

	r = MalData_new(md->type);

	if (md->type == MAL_LIST) {
		for (i = argc - 1; i > 0; i--) {
			MalMulti_add((MalMulti *)r, argv[i]);
		}
	}

	for (i = 0; i < ((MalMulti *)md)->len; i++) {
		MalMulti_add((MalMulti *)r, ((MalMulti *)md)->multi[i]);
	}

	if (md->type == MAL_VECTOR) {
		for (i = 1; i < argc; i++) {
			MalMulti_add((MalMulti *)r, argv[i]);
		}
	}

	return r;
}

CORE_FN_DEF(core_seq_)
{
	MalMulti *r = NULL;
	MalData *md;
	size_t i;

	(void)env;

	FN_ARITY("seq", 1);

	md = argv[0];

	if ( ! MalData_isSequential(md)
	&&   md->type != MAL_STRING
	&&   md->type != MAL_NIL) {
		LOG_ERROR("%s argument %zu must be %s", "seq", 1, "sequentialable");
		E_THROW(E_TYPE, md);
	}

	if (md->type == MAL_NIL
	||  (md->type == MAL_STRING && ((MalString *)md)->len == 0)
	||  (MalData_isSequential(md) && ((MalMulti *)md)->len == 0)) {
		return MalData_new(MAL_NIL);
	}

	if (md->type == MAL_LIST) {
		return md;
	}

	r = (MalMulti *)MalData_new(MAL_LIST);

	if (md->type == MAL_VECTOR) {
		for (i = 0; i < ((MalMulti *)md)->len; i++) {
			MalMulti_add(r, ((MalMulti *)md)->multi[i]);
		}
	}

	if (md->type == MAL_STRING) {
		for (i = 0; i < ((MalString *)md)->len; i++) {
			MalString *glyph = (MalString *)MalData_new(MAL_STRING);
			glyph->string = GC_STRNDUP(((MalString *)md)->string + i, 1);
			glyph->len = 1;
			glyph->alloc = 2;
			MalMulti_add(r, (MalData *)glyph);
		}
	}

	return (MalData *)r;
}

CORE_FN_DEF(core_readline_)
{
	MalString *r;
	MalString *prompt;
	char *line;

	(void)env;

	FN_ARITY("readline", 1);
	FN_ARG_TYPE("readline", 0, MAL_STRING, "string");

	prompt = (MalString *)argv[0];

	line = console_input(prompt->string);
	if (line == NULL) {
		return MalData_new(MAL_NIL);
	}

	r = MalString_new(line);

	free(line);

	return (MalData *)r;
}
