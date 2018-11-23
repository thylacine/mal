#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <gc.h>

#include "logger.h"
#include "types.h"
#include "printer.h"
#include "env.h"
#include "mal.h"
#include "e.h"


static const char *MalType_names_[] = {
	"DATA",
	"MULTI",
	"LIST",
	"VECTOR",
	"HASHMAP",
	"INTEGER",
	"FLOAT",
	"SYMBOL",
	"NIL",
	"TRUE",
	"FALSE",
	"STRING",
	"KEYWORD",
	"ATOM",
	"FUNCTION",
	"CLOSURE",
	"INVALID",
};

const char * const brackets_list = "()";
const char * const brackets_vector = "[]";
const char * const brackets_hashmap = "{}";
const char * const brackets_multi = "<>";

static const size_t MalType_sizes_[] = {
	sizeof(MalData),
	sizeof(MalMulti),
	sizeof(MalList),
	sizeof(MalVector),
	sizeof(MalHashmap),
	sizeof(MalInteger),
	sizeof(MalFloat),
	sizeof(MalSymbol),
	sizeof(MalNil),
	sizeof(MalTrue),
	sizeof(MalFalse),
	sizeof(MalString),
	sizeof(MalKeyword),
	sizeof(MalAtom),
	sizeof(MalFunction),
	sizeof(MalClosure),
};

static MalNil nil_ = { .maldata = { .type = MAL_NIL } };
static MalTrue true_ = { .maldata = { .type = MAL_TRUE } };
static MalFalse false_ = { .maldata = { .type = MAL_FALSE } };


MalData *
MalData_new(MalType t)
{
	MalData *md;
	size_t sz;

	assert(t > MAL_DATA);
	assert(t < MAL_END_);

	/* single-instance and invalid things */
	switch (t) {
	case MAL_NIL:
		return (MalData *)&nil_;

	case MAL_TRUE:
		return (MalData *)&true_;

	case MAL_FALSE:
		return (MalData *)&false_;

	case MAL_DATA:
	case MAL_MULTI:
	case MAL_END_:
		LOG_ERROR("not instantiating abstract type %zu", (size_t)t);
		E_THROW(E_TYPE, NULL);

	default:
		break;
	}

	sz = MalType_sizes_[t];

	md = GC_MALLOC(sz);
	if (md == NULL) {
		LOG_ERROR("%s: %s", "malloc", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}
	memset(md, 0, sz);
	md->type = t;

	return md;
}

MalData *
MalData_clone(MalData *src)
{
	MalData *dst;

	assert(src != NULL);

	dst = MalData_new(src->type);
	switch (src->type) {
	case MAL_DATA:
		*(MalData *)dst = *(MalData *)src;
		break;

	case MAL_MULTI:
	case MAL_LIST:
	case MAL_VECTOR:
		*(MalMulti *)dst = *(MalMulti *)src;
		break;

	case MAL_HASHMAP:
		*(MalHashmap *)dst = *(MalHashmap *)src;
		break;

	case MAL_INTEGER:
		*(MalInteger *)dst = *(MalInteger *)src;
		break;

	case MAL_FLOAT:
		*(MalFloat *)dst = *(MalFloat *)src;
		break;

	case MAL_SYMBOL:
		*(MalSymbol *)dst = *(MalSymbol *)src;
		break;

	case MAL_NIL:
		*(MalNil *)dst = *(MalNil *)src;
		break;

	case MAL_TRUE:
		*(MalTrue *)dst = *(MalTrue *)src;
		break;

	case MAL_FALSE:
		*(MalFalse *)dst = *(MalFalse *)src;
		break;

	case MAL_STRING:
		*(MalString *)dst = *(MalString *)src;
		break;

	case MAL_KEYWORD:
		*(MalKeyword *)dst = *(MalKeyword *)src;
		break;

	case MAL_ATOM:
		*(MalAtom *)dst = *(MalAtom *)src;
		break;

	case MAL_FUNCTION:
		*(MalFunction *)dst = *(MalFunction *)src;
		break;

	case MAL_CLOSURE:
		*(MalClosure *)dst = *(MalClosure *)src;
		break;

	default:
		LOG_ERROR("invalid type %zu", src->type);
		E_THROW(E_TYPE, src);
	}

	return dst;
}

MalString *
MalString_new(const char *str)
{
	MalString *r = (MalString *)MalData_new(MAL_STRING);

	if (str) {
		r->string = GC_STRDUP(str);
		if (r->string == NULL) {
			LOG_ERROR("%s: %s", "strdup", strerror(errno));
			E_THROW(E_RESOURCE, NULL);
		}
		r->len = strlen(str);
		r->alloc = r->len + 1;
	}

	return r;
}

MalSymbol *
MalSymbol_new(const char *sym)
{
	MalSymbol *r = (MalSymbol *)MalData_new(MAL_SYMBOL);

	if (sym) {
		r->symbol = GC_STRDUP(sym);
		if (r->symbol == NULL) {
			LOG_ERROR("%s: %s", "strdup", strerror(errno));
			E_THROW(E_RESOURCE, NULL);
		}
	}

	return r;
}

static
inline
int
MalNil_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_NIL);

	switch (b->type) {
	case MAL_NIL:
	case MAL_FALSE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalFalse_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_FALSE);

	switch (b->type) {
	case MAL_NIL:
	case MAL_FALSE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalTrue_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_TRUE);

	switch (b->type) {
	case MAL_NIL:
	case MAL_FALSE:
		return -1;

	default:
		return 0;
	}
}

static
inline
int
MalSymbol_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_SYMBOL);

	switch (b->type) {
	case MAL_SYMBOL:
		return strcmp(((MalSymbol *)a)->symbol, ((MalSymbol *)b)->symbol);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalKeyword_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_KEYWORD);

	switch (b->type) {
	case MAL_KEYWORD:
		return strcmp(((MalKeyword *)a)->keyword, ((MalKeyword *)b)->keyword);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalString_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_STRING);

	switch (b->type) {
	case MAL_STRING:
		return strcmp(((MalString *)a)->string, ((MalString *)b)->string);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalInteger_compare_(MalData *a, MalData *b)
{
	long double fdiff;
	long long int diff;

	assert(a->type == MAL_INTEGER);

	switch (b->type) {
	case MAL_INTEGER:
		diff = ((MalInteger *)a)->value - ((MalInteger *)b)->value;
		return (diff > 0) - (diff < 0);

	case MAL_FLOAT:
		fdiff = (long double)((MalInteger *)a)->value - ((MalFloat *)b)->value;
		if (fabsl(fdiff) < LDBL_EPSILON) {
			return 0;
		}
		return (fdiff > 0.0L) - (fdiff < 0.0L);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalFloat_compare_(MalData *a, MalData *b)
{
	long double fdiff;

	assert(a->type == MAL_FLOAT);

	switch (b->type) {
	case MAL_FLOAT:
		fdiff = ((MalFloat *)a)->value - ((MalFloat *)b)->value;
		if (fabsl(fdiff) < LDBL_EPSILON) {
			return 0;
		}
		return (fdiff > 0.0L) - (fdiff < 0.0L);

	case MAL_INTEGER:
		fdiff = ((MalFloat *)a)->value - (long double)((MalInteger *)b)->value;
		if (fabsl(fdiff) < LDBL_EPSILON) {
			return 0;
		}
		return (fdiff > 0.0L) - (fdiff < 0.0L);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalFunction_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_FUNCTION);

	switch (b->type) {
	case MAL_FUNCTION:
		return ! (((MalFunction *)a)->fn == ((MalFunction *)b)->fn);

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalClosure_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_CLOSURE);

	switch (b->type) {
	case MAL_CLOSURE:
		return ! (((MalClosure *)a)->env == ((MalClosure *)b)->env
		&&       ((MalClosure *)a)->params == ((MalClosure *)b)->params
		&&       ( ((MalClosure *)a)->body == ((MalClosure *)b)->body
		          || MalData_compare(((MalClosure *)a)->body, ((MalClosure *)b)->body)));

	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalMulti_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_MULTI);

	if (MalData_isSequential(b)) {
		size_t i;
		if (((MalMulti *)a)->len != ((MalMulti *)b)->len) {
			return -1;
		}
		for (i = 0; i < ((MalMulti *)a)->len; i++) {
			if (MalData_compare(((MalMulti *)a)->multi[i], ((MalMulti *)b)->multi[i])) {
				return -1;
			}
		}
		return 0;
	}

	switch (b->type) {
	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalList_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_LIST);

	if (MalData_isSequential(b)) {
		size_t i;
		if (((MalMulti *)a)->len != ((MalMulti *)b)->len) {
			return -1;
		}
		for (i = 0; i < ((MalMulti *)a)->len; i++) {
			if (MalData_compare(((MalMulti *)a)->multi[i], ((MalMulti *)b)->multi[i])) {
				return -1;
			}
		}
		return 0;
	}

	switch (b->type) {
	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalVector_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_VECTOR);

	if (MalData_isSequential(b)) {
		size_t i;
		if (((MalMulti *)a)->len != ((MalMulti *)b)->len) {
			return -1;
		}
		for (i = 0; i < ((MalMulti *)a)->len; i++) {
			if (MalData_compare(((MalMulti *)a)->multi[i], ((MalMulti *)b)->multi[i])) {
				return -1;
			}
		}
		return 0;
	}

	switch (b->type) {
	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalHashmap_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_HASHMAP);

	if (b->type == MAL_HASHMAP) {
		MalHashmap *hma = (MalHashmap *)a,
		           *hmb = (MalHashmap *)b;
		size_t i;

		if (hma->dict.len != hmb->dict.len) {
			return -1;
		}
		for (i = 0; i < hma->dict.len; i++) {
			int c;

			if ((c = MalData_compare(hma->dict.entries[i].key, hmb->dict.entries[i].key)) != 0) {
				return c;
			}

			if ((c = MalData_compare(hma->dict.entries[i].value, hmb->dict.entries[i].value)) != 0) {
				return c;
			}
		}
		return 0;
	}

	switch (b->type) {
	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}
}

static
inline
int
MalData_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_DATA);

	(void)b;

	LOG_ERROR("cannot compare abstract types");
	return -2;
}

static
inline
int
MalAtom_compare_(MalData *a, MalData *b)
{
	assert(a->type == MAL_ATOM);

	if (b->type == MAL_ATOM
	&&  ((MalAtom *)a)->atom == ((MalAtom *)b)->atom) {
		return 0;
	}

	switch (b->type) {
	case MAL_TRUE:
		return 0;

	default:
		return -1;
	}

	return -1;
}

int
MalData_compare(MalData *a, MalData *b)
{
	MalType atype;
	assert(a != NULL);
	assert(b != NULL);

	atype = (a->type <= MAL_END_) ? a->type : MAL_END_;

	switch (atype) {
	case MAL_NIL:
		return MalNil_compare_(a, b);

	case MAL_FALSE:
		return MalFalse_compare_(a, b);

	case MAL_TRUE:
		return MalTrue_compare_(a, b);

	case MAL_SYMBOL:
		return MalSymbol_compare_(a, b);

	case MAL_KEYWORD:
		return MalKeyword_compare_(a, b);

	case MAL_STRING:
		return MalString_compare_(a, b);

	case MAL_INTEGER:
		return MalInteger_compare_(a, b);

	case MAL_FLOAT:
		return MalFloat_compare_(a, b);

	case MAL_FUNCTION:
		return MalFunction_compare_(a, b);

	case MAL_CLOSURE:
		return MalClosure_compare_(a, b);

	case MAL_MULTI:
		return MalMulti_compare_(a, b);

	case MAL_LIST:
		return MalList_compare_(a, b);

	case MAL_VECTOR:
		return MalVector_compare_(a, b);

	case MAL_HASHMAP:
		return MalHashmap_compare_(a, b);

	case MAL_DATA:
		return MalData_compare_(a, b);

	case MAL_ATOM:
		return MalAtom_compare_(a, b);

	case MAL_END_:
		LOG_ERROR("internal error: invalid type %u", a->type);
		return -2;
	}

	return -3; /* will never happen, but it keeps gcc-6.4.0 from warning */
}

size_t
MalMulti_add(MalMulti *md, MalData *data)
{
	const size_t growby = 8;

	assert(md != NULL);
	assert(data != NULL);

	if (md->len >= md->alloc) {
		void *tmpptr;

		tmpptr = GC_REALLOC(md->multi, (md->alloc + growby) * sizeof *md->multi);
		if (tmpptr == NULL) {
			LOG_ERROR("%s: %s", "realloc", strerror(errno));
			E_THROW(E_RESOURCE, NULL);
		}

		md->multi = tmpptr;
		md->alloc += growby;
	}

	md->multi[md->len] = data;
	md->len += 1;

	return md->len;
}

void
MalFunction_fnputf(MalFunction *mf, FILE *f)
{
	unsigned char *p = (unsigned char *)&mf->fn;
	size_t i;

	for (i = 0; i < sizeof mf->fn; i++) {
		fprintf(f, "%02x", p[i]);
	}
}

void
MalClosure_fnputf(MalClosure *mc, FILE *f)
{
	unsigned char *p = (unsigned char *)mc;
	size_t i;

	for (i = 0; i < sizeof *mc; i++) {
		fprintf(f, "%02x", p[i]);
	}
}

inline
int
MalData_isSequential(MalData *md)
{
	assert(md != NULL);

	switch (md->type) {
	case MAL_MULTI:
	case MAL_LIST:
	case MAL_VECTOR:
		return 1;
	default:
		return 0;
	}
}

inline
int
MalData_isNumber(MalData *md)
{
	assert(md != NULL);

	switch (md->type) {
	case MAL_INTEGER:
	case MAL_FLOAT:
		return 1;
	default:
		return 0;
	}
}

inline
int
MalData_isFunction(MalData *md)
{
	assert (md != NULL);

	switch (md->type) {
	case MAL_FUNCTION:
	case MAL_CLOSURE:
		return 1;
	default:
		return 0;
	}
}

const char *
MalData_typeName(MalData *md)
{
	return MalType_names_[md->type > MAL_END_ ? MAL_END_ : md->type];
}

void
md_dumpf(FILE *f, MalData *md)
{
	f = f ? f : stderr;

	fprintf(f, "[MalData (%p)", (void *)md);
	
	if (md) {
		enum MalType t = md->type > MAL_END_ ? MAL_END_ : md->type;

		fprintf(f, " type: %s %d", MalType_names_[t], md->type);

		switch (t) {
		case MAL_DATA:
		case MAL_NIL:
		case MAL_TRUE:
		case MAL_FALSE:
		case MAL_END_:
			break;

		case MAL_MULTI:
		case MAL_LIST:
		case MAL_VECTOR:
		case MAL_HASHMAP:
		{
			MalMulti *x = (MalMulti *)md;
			fprintf(f, " alloc:%zu len:%zu multi:%p",
			        x->alloc,
			        x->len,
			        (void *)x->multi
			);
			break;
		}

		case MAL_INTEGER:
		{
			MalInteger *x = (MalInteger *)md;
			fprintf(f, " value: %lld",
			        x->value);
			break;
		}

		case MAL_FLOAT:
		{
			MalFloat *x = (MalFloat *)md;
			fprintf(f, " value: %Lf",
			        x->value);
			break;
		}

		case MAL_SYMBOL:
		{	
			MalSymbol *x = (MalSymbol *)md;
			fprintf(f, " symbol: '%s'",
			        x->symbol
			);
			break;
		}

		case MAL_KEYWORD:
		{
			MalKeyword *x = (MalKeyword *)md;
			fprintf(f, " keyword: '%s'",
			        x->keyword
			);
			break;
		}

		case MAL_STRING:
		{
			MalString *x = (MalString *)md;
			fprintf(f, " string: '%s'",
			        x->string);
			break;
		}

		case MAL_FUNCTION:
		{
			MalFunction *x = (MalFunction *)md;
			fputs(" fn: ", f);
			MalFunction_fnputf(x, f);
			break;
		}

		case MAL_CLOSURE:
		{
			MalClosure *x = (MalClosure *)md;
			fputs(" fn-closure: ", f);
			if (x->is_macro) {
				fputs("(macro) ", f);
			}
			MalClosure_fnputf(x, f);
			break;
		}

		case MAL_ATOM:
		{
			MalAtom *x = (MalAtom *)md;
			fprintf(f, " atom: (%p) ", (void *)x->atom);
			md_dumpf(f, x->atom);
			break;
		}

		}
	}

	fputs("]\n", f);
}

MalData *
MalFunction_apply(MalData *fn, size_t argc, MalData **argv, MalEnv *env)
{

	assert(fn != NULL);

	switch (fn->type) {
	case MAL_FUNCTION:
		return ((MalFunction *)fn)->fn(argc, argv, env);

	case MAL_CLOSURE:
	{
		MalClosure *mc = (MalClosure *)fn;
		MalEnv *fn_env;

		fn_env = MalEnv_new(mc->env, mc->params, argv, argc);
		fn_env->args = 1;

		return mal_eval(mc->body, fn_env);
	}

	default:
		E_THROW(E_PARSE_FAILURE, fn);
	}

	return NULL;
}

static
int
mal_dictionary_entry_compare_(const void *va, const void *vb)
{
	struct mal_dictionary_entry *a = (struct mal_dictionary_entry *)va,
	                            *b = (struct mal_dictionary_entry *)vb;
	return MalData_compare(a->key, b->key);
}

MalData *
mal_dictionary_set(struct mal_dictionary *dict, MalData *key, MalData *value, int flags)
{
	const size_t growby = 8;
	MalData *r = NULL;
	struct mal_dictionary_entry *entry,
	                            kv = {
		.key = key,
		.value = value,
	};

	assert(dict != NULL);
	assert(key != NULL);
	assert(value != NULL);

	entry = bsearch(&kv, dict->entries, dict->len, sizeof *dict->entries, mal_dictionary_entry_compare_);
	if (entry == NULL) {
		if (dict->len >= dict->alloc) {
			void *tmpptr = GC_REALLOC(dict->entries, (dict->alloc + growby) * sizeof *dict->entries);
			if (tmpptr == NULL) {
				LOG_ERROR("%s: %s", "realloc", strerror(errno));
				E_THROW(E_RESOURCE, NULL);
			}
			dict->entries = tmpptr;
			dict->alloc += growby;
		}
		entry = &dict->entries[dict->len];
		dict->len += 1;
		entry->key = key;
	} else {
		r = entry->value;
	}
	entry->value = value;

	if (! (flags & MAL_DICTIONARY_SET_NOSORT)) {
		qsort(dict->entries, dict->len, sizeof *dict->entries, mal_dictionary_entry_compare_);
	}

	return r;
}

void
mal_dictionary_sort(struct mal_dictionary *dict)
{
	assert(dict != NULL);

	qsort(dict->entries, dict->len, sizeof *dict->entries, mal_dictionary_entry_compare_);
}

MalData *
mal_dictionary_get(struct mal_dictionary *dict, MalData *key)
{
	struct mal_dictionary_entry *entry,
	                            kv = {
		.key = key,
		.value = NULL,
	};

	assert(dict != NULL);
	assert(key != NULL);

	entry = bsearch(&kv, dict->entries, dict->len, sizeof *dict->entries, mal_dictionary_entry_compare_);

	return entry ? entry->value : NULL;
}

MalData *
MalHashmap_set(MalHashmap *hm, MalData *key, MalData *value)
{
	assert(hm != NULL);
	assert(key != NULL);
	assert(value != NULL);

	(void)mal_dictionary_set(&hm->dict, key, value, 0);

	return (MalData *)hm;
}

MalData *
MalHashmap_set_more(MalHashmap *hm, MalData *key, MalData *value)
{
	assert(hm != NULL);
	assert(key != NULL);
	assert(value != NULL);

	(void)mal_dictionary_set(&hm->dict, key, value, MAL_DICTIONARY_SET_NOSORT);

	return (MalData *)hm;
}

void
MalHashmap_set_done(MalHashmap *hm)
{
	assert(hm != NULL);

	mal_dictionary_sort(&hm->dict);
}

MalData *
MalHashmap_get(MalHashmap *hm, MalData *key)
{
	assert(hm != NULL);
	assert(key != NULL);

	return mal_dictionary_get(&hm->dict, key);
}

MalList *
MalHashmap_keys(MalHashmap *hm)
{
	MalList *keys;
	size_t i;

	assert(hm != NULL);

	keys = (MalList *)MalData_new(MAL_LIST);

	for (i = 0; i < hm->dict.len; i++) {
		MalMulti_add((MalMulti *)keys, hm->dict.entries[i].key);
	}

	return keys;
}

MalList *
MalHashmap_vals(MalHashmap *hm)
{
	MalList *values;
	size_t i;

	assert(hm != NULL);

	values = (MalList *)MalData_new(MAL_LIST);

	for (i = 0; i < hm->dict.len; i++) {
		MalMulti_add((MalMulti *)values, hm->dict.entries[i].value);
	}

	return values;
}
