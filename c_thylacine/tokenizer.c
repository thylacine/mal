#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "tokenizer.h"
#include "e.h"

static
inline
Token
new_token_(const char *s, size_t n) {
	Token t = GC_STRNDUP(s, n);
	if (t == NULL) {
		LOG_ERROR("%s: %s", "strndup", strerror(errno));
		E_THROW(E_RESOURCE, NULL);
	}
	return t;
}

enum tokenizer_behavior {
	TOK_CONSUME_WHITESPACE = 0,
	TOK_START_TOKEN,
	TOK_LOCATE_TOKEN_END,
	TOK_QUOTED,
	TOK_COMMENT,
};

struct tokenizer_state {
	Token *tokens;
	size_t tokens_alloc;
	size_t tokens_n;
	size_t str_len;
	size_t tok_start;
	size_t offset;
	enum tokenizer_behavior behavior;
};

Token *
tokenize(const char *str, int *unfinished_quote) {
	const size_t tokens_growby = 128;
	struct tokenizer_state state = {
		.tokens = NULL,
		.tokens_alloc = 0,
		.tokens_n = 0,
		.behavior = TOK_CONSUME_WHITESPACE,
		.str_len = 0,
		.tok_start = 0,
		.offset = 0,
	};

	assert(str != NULL);

	state.str_len = strlen(str);
	*unfinished_quote = 0;

	E_START
	case E_TRY:
		do {
			if (state.tokens_n >= state.tokens_alloc) {
				void *tmpptr = GC_REALLOC(state.tokens, sizeof *state.tokens * (state.tokens_alloc + tokens_growby));
				if (tmpptr == NULL) {
					LOG_ERROR("%s: %s", "realloc", strerror(errno));
					E_THROW(E_RESOURCE, NULL);
				}
				state.tokens = tmpptr;
				state.tokens_alloc += tokens_growby;
			}

			switch (state.behavior) {
			case TOK_CONSUME_WHITESPACE:
				if (strchr(", \t\n", str[state.offset]) == NULL) {
					state.behavior = TOK_START_TOKEN;
					break;
				}
				state.offset += 1;
				break;

			case TOK_START_TOKEN:
				state.tok_start = state.offset;
				if (str[state.offset] == ';') {
					state.behavior = TOK_COMMENT;
					state.offset += 1;
					break;
				}
				if (str[state.offset] == '"') {
					state.behavior = TOK_QUOTED;
					state.offset += 1;
					*unfinished_quote = 1;
					break;
				}
				if (strchr("()[]{}'`~^@", str[state.offset])) {
					size_t tok_len = (str[state.offset] == '~' && str[state.offset + 1] == '@') ? 2 : 1;
					state.tokens[state.tokens_n] = new_token_(str + state.tok_start, tok_len);
					state.tokens_n += 1;
					state.offset += tok_len;
					state.behavior = TOK_CONSUME_WHITESPACE;
					break;
				}
				state.behavior = TOK_LOCATE_TOKEN_END;
				break;

			case TOK_COMMENT:
				if (! str[state.offset]
				||  str[state.offset] == '\n') {
					/* ignore comments entirely in returned token list */
					state.offset += 1;
					state.tok_start = state.offset;
					state.behavior = TOK_CONSUME_WHITESPACE;
					break;
				}
				state.offset += 1;
				break;

			case TOK_QUOTED:
				if (str[state.offset] == '"') {
					size_t tok_len = state.offset - state.tok_start + 1;
					state.tokens[state.tokens_n] = new_token_(str + state.tok_start, tok_len);
					state.tokens_n += 1;
					state.offset += 1;
					state.tok_start = state.offset;
					state.behavior = TOK_CONSUME_WHITESPACE;
					*unfinished_quote = 0;
					break;
				}
				if (str[state.offset] == '\\' && str[state.offset + 1]) {
					state.offset += 2;
					break;
				}
				state.offset += 1;
				break;

			case TOK_LOCATE_TOKEN_END:
				if (strchr(" \t\n,;()[]{}'`\"", str[state.offset]) || ! str[state.offset]) {
					size_t token_len = state.offset - state.tok_start;
					if (token_len) {
						state.tokens[state.tokens_n] = new_token_(str + state.tok_start, token_len);
						state.tokens_n += 1;
						state.tok_start = state.offset;
						state.behavior = TOK_CONSUME_WHITESPACE;
						break;
					}
				}
				state.offset += 1;
				break;
			}

		} while (state.offset <= state.str_len);

		state.tokens[state.tokens_n] = NULL;
		break;

	case E_RESOURCE:
		state.tokens = NULL;
		state.tokens_n = 0;
		E_THROW_UP(__e, NULL);
	E_DONE

	return state.tokens;
}

#ifdef TEST
int
main(int argc, char **argv) {
	struct test {
		const char *in;
		Token expect[16];
	} *test, tests[] = {
		{ .in = "",
		  .expect = { NULL } },

		{ .in = "   ",
		  .expect = { NULL } },

		{ .in = "1",
		  .expect = { "1", NULL } },

		{ .in = "1   ",
		  .expect = {"1", NULL } },

		{ .in = ", , ,1",
		  .expect = { "1", NULL } },

		{ .in = ";comment",
		  .expect = { NULL } },

		{ .in = ";",
		  .expect = { NULL } },

		{ .in = "word ; comment",
		  .expect = { "word", NULL } },

		{ .in = "1;",
		  .expect = { "1", NULL } },

		{ .in = "word",
		  .expect = { "word", NULL } },

		{ .in = "\"word\"",
		  .expect = { "\"word\"", NULL } },

		{ .in = "(\"word\" \"word\")",
		  .expect = { "(", "\"word\"", "\"word\"", ")", NULL } },

		{ .in = "(1 2 3)",
		  .expect = { "(", "1", "2", "3", ")", NULL } },

		{ .in = "'thing",
		  .expect = { "'", "thing", NULL } },

		{ .in = " @@@,,,~@ !",
		  .expect = { "@", "@", "@", "~@", "!", NULL } },

		{ .in = "~@(1 2 3)",
		  .expect = { "~@", "(", "1", "2", "3", ")", NULL } },

		{ .in = "~1",
		  .expect = { "~", "1", NULL } },

		{ .in = "(1 2 3)\n(4 5 6)",
		  .expect = { "(", "1", "2", "3", ")", "(", "4", "5", "6", ")", NULL } },

		{ .in = "(+ (+ 1\n2) 3)",
		  .expect = { "(", "+", "(", "+", "1", "2", ")", "3", ")", NULL } },

		{ .in = "(+\n 1 ; one\n 2 ; two\n)",
		  .expect = { "(", "+", "1", "2", ")", NULL } },


		{ .in = NULL, .expect = { NULL } },
	};
	size_t total = 0,
	       pass = 0,
	       fail = 0;
	(void) argc, (void) argv;

	for (test = tests; test->in; test++, total++) {
		Token *tokens, *t, *e;
		int q;

		fprintf(stderr, "input: '%s'\n", test->in);
		tokens = tokenize(test->in, &q);
		fprintf(stderr, "output:\n");
		for (t = tokens, e = &(test->expect[0]); *t; t++, e++) {
			fprintf(stderr, "  got: '%s' wanted: '%s'\n", *t, *e);
			if (strcmp(*t, *e)) {
				break;
			}
		}
		if (*t == *e) {
			pass++;
		} else {
			fail++;
		}
		fprintf(stderr, "%s\n--\n", (*t == *e) ? "PASS" : "FAIL");
	}
	fprintf(stderr, "%zu tests, %zu pass, %zu fail\n", total, pass, fail);
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}
#endif /* TEST */
