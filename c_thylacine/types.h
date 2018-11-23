#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdlib.h>
#include <stdio.h>

extern const char * const brackets_list;
extern const char * const brackets_vector;
extern const char * const brackets_hashmap;
extern const char * const brackets_multi;

typedef enum MalType {
	MAL_DATA = 0, /* abstract */
	MAL_MULTI, /* abstract */
	MAL_LIST,
	MAL_VECTOR,
	MAL_HASHMAP,
	MAL_INTEGER,
	MAL_FLOAT,
	MAL_SYMBOL,
	MAL_NIL,
	MAL_TRUE,
	MAL_FALSE,
	MAL_STRING,
	MAL_KEYWORD,
	MAL_ATOM,
	MAL_FUNCTION,
	MAL_CLOSURE,
	MAL_END_
} MalType;

struct MalEnv;

struct mal_dictionary_entry {
	struct MalData *key;
	struct MalData *value;
};
struct mal_dictionary {
	struct mal_dictionary_entry *entries;
	size_t len;
	size_t alloc;
};

#define MAL_DICTIONARY_SET_NOSORT (1<<0)
struct MalData * mal_dictionary_set(struct mal_dictionary *, struct MalData *, struct MalData *, int);
void mal_dictionary_sort(struct mal_dictionary *);
struct MalData * mal_dictionary_get(struct mal_dictionary *, struct MalData *);

typedef struct MalData {
	enum MalType type;
	struct MalData *meta;
} MalData;

typedef struct MalMulti {
	struct MalData maldata;
	size_t alloc;
	size_t len;
	struct MalData **multi;
} MalMulti;

typedef struct MalList {
	struct MalMulti malmulti;
} MalList;

typedef struct MalInteger {
	struct MalData maldata;
	long long int value;
} MalInteger;

typedef struct MalFloat {
	struct MalData maldata;
	long double value;
} MalFloat;

typedef struct MalSymbol {
	struct MalData maldata;
	const char *symbol;
} MalSymbol;

typedef struct MalNil {
	struct MalData maldata;
} MalNil;

typedef struct MalTrue {
	struct MalData maldata;
} MalTrue;

typedef struct MalFalse {
	struct MalData maldata;
} MalFalse;

typedef struct MalString {
	struct MalData maldata;
	size_t alloc;
	size_t len;
	const char *string;
} MalString;

typedef struct MalKeyword {
	struct MalData maldata;
	const char *keyword;
} MalKeyword;

typedef struct MalVector {
	struct MalMulti malmulti;
} MalVector;

typedef struct MalHashmap {
	struct MalData maldata;
	struct mal_dictionary dict;
} MalHashmap;

typedef struct MalFunction {
	struct MalData maldata;
	struct MalData * (*fn)(size_t, MalData **, struct MalEnv *);
} MalFunction;

typedef struct MalClosure {
	struct MalData maldata;
	struct MalEnv *env;
	struct MalData *params;
	struct MalData *body;
	unsigned int is_macro;
} MalClosure;

typedef struct MalAtom {
	struct MalData maldata;
	struct MalData *atom;
} MalAtom;



size_t MalMulti_add(MalMulti *, MalData *);
MalData * MalData_new(MalType);
MalData * MalData_clone(MalData *);
MalString * MalString_new(const char *);
MalSymbol * MalSymbol_new(const char *);
int MalData_compare(MalData *, MalData *);
void MalFunction_fnputf(MalFunction *, FILE *);
void MalClosure_fnputf(MalClosure *, FILE *);
int MalData_isNumber(MalData *);
int MalData_isSequential(MalData *);
int MalData_isFunction(MalData *);
const char * MalData_typeName(MalData *);
MalData * MalFunction_apply(MalData *fn, size_t argc, MalData **argv, struct MalEnv *env);

MalData * MalHashmap_get(MalHashmap *, MalData *);
MalData * MalHashmap_set(MalHashmap *, MalData *, MalData *);
MalData * MalHashmap_set_more(MalHashmap *, MalData *, MalData *);
void MalHashmap_set_done(MalHashmap *);
MalList * MalHashmap_keys(MalHashmap *);
MalList * MalHashmap_vals(MalHashmap *);

void md_dumpf(FILE *f, MalData *md);

#endif
