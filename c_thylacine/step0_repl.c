#include <stdlib.h>
#include <stdio.h>

#include "console_input.h"

char *
mal_read(const char *prompt)
{
	char *line;

	line = console_input(prompt);

	return line;
}

char *
mal_eval(char *arg)
{
	return arg;
}

char *
mal_print(char *s)
{
	if (s) {
		puts(s);
	}
	return s;
}

char *
mal_rep(void)
{
	const char *prompt = "user> ";

	return mal_print(mal_eval(mal_read(prompt)));
}

int
main(int argc, char *argv[])
{
	char *x;

	(void)argc;

	console_input_init(argv[0]);

	while ((x = mal_rep())) {
		free(x);
	}

	puts("goodbye");

	console_input_fini();

	exit(EXIT_SUCCESS);
}
