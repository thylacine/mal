#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "e.h"
#include "types.h"
#include "tokenizer.h"
#include "reader.h"


#define D(...) do { \
	fprintf(stderr, __VA_ARGS__); \
} while (0)


struct reader_state {
	Token *tokens;
	size_t position;
};

static struct reader_state state_ = {
	.tokens = NULL,
	.position = 0,
};

/* mutually recursive */
static MalData * read_multi_(struct reader_state *, MalType, const char *);
static MalData * read_hashmap_(struct reader_state *);
static MalData * read_form_(struct reader_state *);


static
void
reader_state_reset_(struct reader_state *s)
{
	assert(s != NULL);

	s->tokens = NULL;
	s->position = 0;
}

void
reader_init(void)
{
	reader_state_reset_(&state_);
}

void
reader_fini(void)
{
	reader_state_reset_(&state_);
}

static
inline
Token
reader_peek_(struct reader_state *s)
{
	assert(s != NULL);
	assert(s->tokens != NULL);

	return s->tokens[s->position];
}

static
inline
Token
reader_next_(struct reader_state *s)
{
	Token token;

	assert(s != NULL);
	assert(s->tokens != NULL);

	token = s->tokens[s->position];
	s->tokens[s->position] = NULL;
	s->position += 1;

	return token;
}

static
MalData *
read_hashmap_(struct reader_state *s)
{
	Token t;
	MalHashmap *hm;
	MalData *key = NULL;

	assert(s != NULL);

	hm = (MalHashmap *)MalData_new(MAL_HASHMAP);

	t = reader_next_(s);
	assert(t != NULL);
	if (*t != brackets_hashmap[0]) {
		LOG_ERROR("expected '%c, got '%s'", brackets_hashmap[0], t);
		E_THROW(E_PARSE_FAILURE, NULL);
	}

	for (;;) {
		t = reader_peek_(s);
		if (t == NULL) {
			LOG_ERROR("expected '%c', got EOF", brackets_hashmap[1]);
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		if (*t == brackets_hashmap[1]) {
			if (key != NULL) {
				LOG_ERROR("expected value, got '%s'", t);
				E_THROW(E_PARSE_FAILURE, NULL);
			} else {
				t = reader_next_(s);
				break;
			}
		}

		if (key == NULL) {
			key = read_form_(s);
		} else {
			MalHashmap_set_more(hm, key, read_form_(s));
			key = NULL;
		}
	}

	MalHashmap_set_done(hm);

	return (MalData *)hm;
}

static
MalData *
read_multi_(struct reader_state *s, MalType type, const char *brackets)
{
	Token t = NULL;
	MalMulti *md = NULL;

	assert(s != NULL);
	assert(brackets != NULL);

	md = (MalMulti *)MalData_new(type);

	t = reader_next_(s);
	assert(t != NULL);
	if (*t != brackets[0]) {
		LOG_ERROR("expected '%c, got '%s'", brackets[0], t);
		E_THROW(E_PARSE_FAILURE, NULL);
	}

	for (;;) {
		t = reader_peek_(s);
		if (t == NULL) {
			LOG_ERROR("expected '%c', got EOF", brackets[1]);
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		if (*t == brackets[1]) {
			t = reader_next_(s);
			break;
		}

		MalMulti_add(md, read_form_(s));
	}

	return (MalData *)md;
}

static
MalData *
read_atom_(struct reader_state *s)
{
	Token t = NULL;
	MalData *atom = NULL;

	assert(s != NULL);

	t = reader_next_(s);
	assert(t != NULL);

	/* is it a simple match? */
	if (strcmp(t, "nil") == 0) {
		atom = MalData_new(MAL_NIL);
	} else if (strcmp(t, "true") == 0) {
		atom = MalData_new(MAL_TRUE);
	} else if (strcmp(t, "false") == 0) {
		atom = MalData_new(MAL_FALSE);
	}
	if (atom == NULL) {
		/* is it an integer? */
		MalInteger *number;
		char *endp;
		long long int v;

		errno = 0;
		v = strtoll(t, &endp, 0);
		if (errno == ERANGE) {
			LOG_ERROR("integer value out of range");
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		if (*t && *endp == '\0') {
			number = (MalInteger *)MalData_new(MAL_INTEGER);
			number->value = v;

			atom = (MalData *)number;
		}
	}
	if (atom == NULL) {
		/* is it a float? */
		MalFloat *number;
		char *endp;
		long double v;

		errno = 0;
		v = strtold(t, &endp);
		if (errno == ERANGE) {
			LOG_ERROR("float value out of range");
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		if (*t && *endp == '\0') {
			number = (MalFloat *)MalData_new(MAL_FLOAT);
			number->value = v;

			atom = (MalData *)number;
		}

	}
	if (atom == NULL) {
	/* default to a symbol */
		MalSymbol *symbol;

		symbol = (MalSymbol *)MalData_new(MAL_SYMBOL);
		symbol->symbol = t;

		atom = (MalData *)symbol;
	}

	return atom;
}

static
MalData *
read_keyword_(struct reader_state *s)
{
	Token t = NULL;
	MalKeyword *k = NULL;

	t = reader_next_(s);
	assert(t != NULL);

	k = (MalKeyword *)MalData_new(MAL_KEYWORD);
	k->keyword = t;

	return (MalData *)k;
}

static
const char *
unescape_string_(char *str)
{
	char *s, *d;

	assert(str != NULL);

	/* skip opening quote */
	str = GC_STRDUP(str + (*str == '"' ? 1 : 0));
	if (str == NULL) {
		LOG_ERROR("%s: %s", "strdup", strerror(errno));
		return NULL;
	}

	for (s = d = str; *s; s++, d++) {
		/* skip closing quote */
		if (*s == '"' && *(s + 1) == '\0') {
			s++;
		} else if (*s == '\\') {
			switch (*(s+1)) {
			case '\0':
			case '\\':
			case '"':
				s++;
				break;
			case 'n':
				s++;
				*d = '\n';
				continue;
			}
		}
		*d = *s;
	}

	return str;
}

static
MalData *
read_string_(struct reader_state *s)
{
	Token t = NULL;
	MalString *str = NULL;
	size_t last;

	t = reader_next_(s);
	assert(t != NULL);

	last = strlen(t) - 1;
	if (*t != '"'
	||  t[last] != '"') {
		LOG_ERROR("peculiar string '%c' '%c'", *t, t[last]);
		E_THROW(E_PARSE_FAILURE, NULL);
	}

	str = (MalString *)MalData_new(MAL_STRING);
	str->string = unescape_string_((char *)t);
	if (str->string == NULL) {
		E_THROW(E_RESOURCE, NULL);
	}
	str->len = strlen(str->string);
	str->alloc = str->len + 1;

	return (MalData *)str;
}

static
MalData *
read_macro_(struct reader_state *s)
{
	Token t;
	MalMulti *md;
	MalSymbol *symbol = NULL;
	MalData *d = NULL;

	t = reader_next_(s);
	assert(t != NULL);

	md = (MalMulti *)MalData_new(MAL_LIST);

	switch (*t) {
	case '\'':
		symbol = MalSymbol_new("quote");
		break;
	case '`':
		symbol = MalSymbol_new("quasiquote");
		break;
	case '~':
		switch (*(t + 1)) {
		case '\0':
			symbol = MalSymbol_new("unquote");
			break;
		case '@':
			symbol = MalSymbol_new("splice-unquote");
			break;
		default:
			LOG_ERROR("unexpected '%c' '%s'", *(t+1), t);
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		break;
	case '@':
		symbol = MalSymbol_new("deref");
		break;
	case '^':
		symbol = MalSymbol_new("with-meta");
		break;
	default:
		LOG_ERROR("unexpected '%s'", t);
		E_THROW(E_PARSE_FAILURE, NULL);
	}

	MalMulti_add(md, (MalData *)symbol);

	if (*t == '^') {
		d = read_form_(s);
		if (d == NULL) {
			E_THROW(E_PARSE_FAILURE, NULL);
		}
		if (reader_peek_(s) == NULL) {
			E_THROW(E_PARSE_FAILURE, NULL);
		}
	}

	MalMulti_add(md, read_form_(s));

	if (*t == '^') {
		MalMulti_add(md, d);
	}

	return (MalData *)md;
}

static
MalData *
read_form_(struct reader_state *s)
{
	Token t = NULL;
	MalData *d = NULL;

	t = reader_peek_(s);
	if (t == NULL) {
		return NULL;
	}

	switch (*t) {
	case ')':
	case ']':
	case '}':
		LOG_ERROR("unexpected '%c'", *t);
		E_THROW(E_PARSE_FAILURE, NULL);
		break;

	case '(':
		d = read_multi_(s, MAL_LIST, brackets_list);
		break;

	case '[':
		d = read_multi_(s, MAL_VECTOR, brackets_vector);
		break;

	case '{':
		d = read_hashmap_(s);
		break;

	case ':':
		d = read_keyword_(s);
		break;

	case '"':
		d = read_string_(s);
		break;

	case '\'':
	case '`':
	case '~':
	case '^':
	case '@':
		d = read_macro_(s);
		break;

	default:
		d = read_atom_(s);
		break;
	}

	return d;
}

MalData *
read_str(const char *str)
{
	MalData *md = NULL;
	int unclosed_quote;

	if (str == NULL) {
		return NULL;
	}

	state_.tokens = tokenize(str, &unclosed_quote);
	state_.position = 0;

	if (unclosed_quote) {
		LOG_ERROR("expected '%c'", '"');
		reader_state_reset_(&state_);
		E_THROW(E_PARSE_FAILURE, NULL);
	}

	E_START
	case E_TRY:
		md = read_form_(&state_);
		break;

	case E_FINALLY:
		reader_state_reset_(&state_);
		break;
	E_DONE

	return md;
}
