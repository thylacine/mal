#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "logger.h"
#include "types.h"
#include "printer.h"
#include "strbuf.h"

#define D(...) do { \
	fprintf(stderr, __VA_ARGS__); \
	fflush(stderr); \
} while (0)

static
void
pr_dictionary_(struct mal_dictionary *dict, unsigned int flags, const char *brackets)
{
	size_t i;

	putchar(brackets[0]);
	for (i = 0; i < dict->len; i++) {
		pr_str(dict->entries[i].key, flags);
		putchar(' ');
		pr_str(dict->entries[i].value, flags);
		if (i + 1 < dict->len) {
			putchar(' ');
		}
	}
	putchar(brackets[1]);
}

static
void
pr_multi_(MalMulti *m, unsigned int flags, const char *brackets)
{
	size_t i;

	putchar(brackets[0]);
	for (i = 0; i < m->len; i++) {
		pr_str(m->multi[i], flags);
		if (i + 1 < m->len) {
			putchar(' ');
		}
	}
	putchar(brackets[1]);
}

static
int
pr_dictionary_buf_(char **buf, size_t *buf_alloc, size_t *buf_used, struct mal_dictionary *dict, unsigned int flags, const char *brackets)
{
	int l;
	size_t i;

	l = strbuf_appendf(buf, buf_alloc, buf_used, "%c", brackets[0]);
	if (l < 0) {
		return -1;
	}

	for (i = 0; i < dict->len; i++) {
		pr_str_buf(buf, buf_alloc, buf_used, dict->entries[i].key, flags);
		l = strbuf_appendf(buf, buf_alloc, buf_used, " ");
		if (l < 0) {
			return -1;
		}
		pr_str_buf(buf, buf_alloc, buf_used, dict->entries[i].value, flags);
		if (i + 1 < dict->len) {
			l = strbuf_appendf(buf, buf_alloc, buf_used, " ");
			if (l < 0) {
				return -1;
			}
		}

	}
	l = strbuf_appendf(buf, buf_alloc, buf_used, "%c", brackets[1]);
	if (l < 0) {
		return -1;
	}

	return 0;
}

static
int
pr_multi_buf_(char **buf, size_t *buf_alloc, size_t *buf_used, MalMulti *m, unsigned int flags, const char *brackets)
{
	int l;
	size_t i;

	l = strbuf_appendf(buf, buf_alloc, buf_used, "%c", brackets[0]);
	if (l < 0) {
		return -1;
	}

	for (i = 0; i < m->len; i++) {
		pr_str_buf(buf, buf_alloc, buf_used, m->multi[i], flags);
		if (i + 1 < m->len) {
			l = strbuf_appendf(buf, buf_alloc, buf_used, " ");
			if (l < 0) {
				return -1;
			}
		}
	}
	l = strbuf_appendf(buf, buf_alloc, buf_used, "%c", brackets[1]);
	if (l < 0) {
		return -1;
	}

	return 0;
}

static
void
pr_escaped_str_(const char *str)
{
	const char esc[] = "\"\n\\";

	if (str) {
		do {
			size_t skip;
			int l;

			skip = strcspn(str, esc);

			l = printf("%.*s", (int)skip, str);

			if (l < 0) {
				LOG_ERROR("%s: %s", "printf", strerror(errno));
				abort();
			}
			str += l;

			if (*str) {
				putchar('\\');
				switch (*str) {
				case '\n':
					putchar('n');
					break;
				default:
					putchar(*str);
				}
				str++;
			}
		} while (*str);
	}
}

static
int
pr_escaped_str_buf_(char **buf, size_t *buf_alloc, size_t *buf_used, const char *str)
{
	const char esc[] = "\"\n\\";

	do {
		size_t skip;
		int l;

		skip = strcspn(str, esc);
		l = strbuf_appendf(buf, buf_alloc, buf_used, "%.*s", (int)skip, str);
		if (l < 0) {
			return -1;
		}

		str += l;

		if (*str) {
			char c = *str;
			l = strbuf_appendf(buf, buf_alloc, buf_used, "\\");
			if (l < 0) {
				return -1;
			}
			switch (*str) {
			case '\n':
				c = 'n';
				break;
			}
			l = strbuf_appendf(buf, buf_alloc, buf_used, "%c", c);
			if (l < 0) {
				return -1;
			}
			str++;
		}
	} while (*str);

	return 0;
}

int
pr_str_buf(char **buf, size_t *buf_alloc, size_t *buf_used, MalData *d, unsigned int flags)
{
	assert(d != NULL);
	assert(buf != NULL);
	assert(buf_alloc != NULL);
	assert(buf_used != NULL);

	int l;
	int k;

	if (flags & PR_STR_DEBUG) {
		md_dumpf(NULL, d);
	}

	switch (d->type > MAL_END_ ? MAL_END_ : d->type) {
	case MAL_INTEGER:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "%lld", ((MalInteger *)d)->value);
		break;

	case MAL_FLOAT:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "%Lf", ((MalFloat *)d)->value);
		break;

	case MAL_SYMBOL:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "%s", ((MalSymbol *)d)->symbol);
		break;

	case MAL_NIL:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "nil");
		break;

	case MAL_TRUE:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "true");
		break;

	case MAL_FALSE:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "false");
		break;

	case MAL_STRING:
		if (flags & PR_STR_READABLY) {
			l = strbuf_appendf(buf, buf_alloc, buf_used, "\"");
			k = pr_escaped_str_buf_(buf, buf_alloc, buf_used, ((MalString *)d)->string);
			if (k < 0) {
				return k;
			}
			l += k;
			k = strbuf_appendf(buf, buf_alloc, buf_used, "\"");
			if (k < 0) {
				return k;
			}
			l += k;
		} else {
			l = strbuf_appendf(buf, buf_alloc, buf_used, "%s", ((MalString *)d)->string);
		}
		break;

	case MAL_KEYWORD:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "%s", ((MalKeyword *)d)->keyword);
		break;

	case MAL_MULTI:
		l = pr_multi_buf_(buf, buf_alloc, buf_used, (MalMulti* )d, flags, brackets_multi);
		break;

	case MAL_LIST:
		l = pr_multi_buf_(buf, buf_alloc, buf_used, (MalMulti *)d, flags, brackets_list);
		break;

	case MAL_VECTOR:
		l = pr_multi_buf_(buf, buf_alloc, buf_used, (MalMulti *)d, flags, brackets_vector);
		break;

	case MAL_HASHMAP:
		l = pr_dictionary_buf_(buf, buf_alloc, buf_used, &((MalHashmap *)d)->dict, flags, brackets_hashmap);
		break;

	case MAL_ATOM:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "(atom ");
		if (((MalAtom *)d)->atom->type == MAL_ATOM) {
			k = strbuf_appendf(buf, buf_alloc, buf_used, "#atom#");
		} else {
			k = pr_str_buf(buf, buf_alloc, buf_used, ((MalAtom *)d)->atom, flags);
		}
		if (k < 0) {
			return k;
		}
		l += k;
		k = strbuf_appendf(buf, buf_alloc, buf_used, ")");
		if (k < 0) {
			return k;
		}
		break;

	case MAL_FUNCTION:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "#fn#");
		break;

	case MAL_CLOSURE:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "#fn-closure");
		k = strbuf_appendf(buf, buf_alloc, buf_used, " macro");
		if (k < 0) {
			return k;
		}
		l += k;
		k = strbuf_appendf(buf, buf_alloc, buf_used, "#");
		if (k < 0) {
			return k;
		}
		l += k;
		break;

	case MAL_DATA:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "#abstract#");
		break;

	case MAL_END_:
	default:
		l = strbuf_appendf(buf, buf_alloc, buf_used, "[unknown type %d]", d->type);
		break;
	}

	return l;
}

MalData *
pr_str(MalData *d, unsigned int flags)
{
	if (flags & PR_STR_DEBUG) {
		md_dumpf(NULL, d);
	}

	if (d) {
		enum MalType t = d->type > MAL_END_ ? MAL_END_ : d->type;

		switch (t) {
		case MAL_INTEGER:
			printf("%lld", ((MalInteger *)d)->value);
			break;

		case MAL_FLOAT:
			printf("%Lf", ((MalFloat *)d)->value);
			break;

		case MAL_SYMBOL:
			printf("%s", ((MalSymbol *)d)->symbol);
			break;

		case MAL_NIL:
			printf("nil");
			break;

		case MAL_TRUE:
			printf("true");
			break;

		case MAL_FALSE:
			printf("false");
			break;

		case MAL_STRING:
			if (flags & PR_STR_READABLY) {
				putchar('"');
				pr_escaped_str_(((MalString *)d)->string);
				putchar('"');
			} else {
				fputs(((MalString *)d)->string, stdout);
			}
			break;

		case MAL_KEYWORD:
			printf("%s", ((MalKeyword *)d)->keyword);
			break;

		case MAL_MULTI:
			pr_multi_((MalMulti* )d, flags, brackets_multi);
			break;

		case MAL_LIST:
			pr_multi_((MalMulti *)d, flags, brackets_list);
			break;

		case MAL_VECTOR:
			pr_multi_((MalMulti *)d, flags, brackets_vector);
			break;

		case MAL_HASHMAP:
			pr_dictionary_(&((MalHashmap *)d)->dict, flags, brackets_hashmap);
			break;

		case MAL_ATOM:
			printf("(atom ");
			pr_str(((MalAtom *)d)->atom, flags);
			printf(")");
			break;

		case MAL_FUNCTION:
			printf("#fn");
			MalFunction_fnputf((MalFunction *)d, stdout);
			printf("#");
			break;

		case MAL_CLOSURE:
			printf("#fn-closure");
			if (((MalClosure *)d)->is_macro) {
				printf(" macro");
			}
			printf("#");
			break;

		case MAL_DATA:
			printf("#abstract#");
			break;

		case MAL_END_:
		default:
			printf("[unknown type %d]", d->type);
			break;
		}
	}

	return d;
}
