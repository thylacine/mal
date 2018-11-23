#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

#include "logger.h"
#include "console_input.h"
#include "e.h"
#include "types.h"
#include "printer.h"
#include "reader.h"

#define VERSION_STR "0.0.0"

static const char * const id_ = "v" VERSION_STR;

struct mal_options {
	int verbosity;
} mal_options = {
	.verbosity = 0,
};

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
mal_eval(MalData *ast)
{
	return ast;
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
mal_rep(void)
{
	const char *prompt = "user> ";

	return mal_print(mal_eval(mal_read(prompt)));
}

int
main(int argc, char *argv[])
{
	int c;

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

	reader_init();

	for (;;) {
		MalData *x = NULL;
		int eof = 0;

		E_START
		case E_TRY:
			x = mal_rep();
			break;

		case E_EOF:
			eof = 1;
			x = NULL;
			break;

		case E_PARSE_FAILURE:
			fprintf(stderr, "invalid input\n");
			x = NULL;
			break;
		E_DONE

		if (x || eof) {
			fputc('\n', stdout);
		}

		if (eof) {
			break;
		}
	}

	puts("goodbye");

	console_input_fini();

	exit(EX_OK);
}
