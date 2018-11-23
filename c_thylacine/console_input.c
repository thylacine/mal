#include <stdlib.h>

#ifdef HAVE_READLINE
#	include <readline/history.h>
#	include <readline/readline.h>
#else /* standard input */
#	include <stdio.h>
#	include <string.h>
#	include <errno.h>
#	define BUF_SZ ((1<<16))
#endif

#include "console_input.h"

#ifdef HAVE_READLINE
void
console_input_init(const char *prog)
{
	(void)prog;
}

void
console_input_fini(void)
{
}

char *
console_input(const char *prompt)
{
	char *line;

	line = readline(prompt);

	return line;
}

void
console_input_history_add(const char *line)
{
	add_history(line);
}

#else /* standard input */

void
console_input_init(const char *prog)
{
	(void)prog;
}

void
console_input_fini(void)
{
}

char *
console_input(const char *prompt) {
	static char buf[BUF_SZ];
	char *line, *x;

	fputs(prompt, stdout);
	if (fgets(buf, sizeof buf, stdin) == NULL) {
		if (ferror(stdin)) {
			fprintf(stderr, "%s: %s", "fgets", strerror(errno));
		}
		return NULL;
	}

	for (x = buf; *x; x++) {
		if (*x == '\n') {
			*x = '\0';
		}
	}

	line = strdup(buf);
	if (line == NULL) {
		fprintf(stderr, "%s: %s", "strdup", strerror(errno));
	}

	return line;
}

void
console_input_history_add(const char *line)
{
	(void)line;
}

#endif
