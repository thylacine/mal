#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/queue.h>

#include "logger.h"

struct logger_handler {
	SLIST_ENTRY(logger_handler) next;
	logger_vhandler_fn *fn;
	void *data;
	logger_vhandler_fini_fn *fini_fn;
};

static SLIST_HEAD(logger_handler_head, logger_handler) logger_handlers[L_END_];

static void logger_default_console_(void *, enum logger_level, const char *, const char *, int, const char *, va_list);

logger_vhandler_fn *logger_default_console = logger_default_console_;

const char * logger_level_names[] = {
	"CRITICAL",
	"ERROR",
	"WARNING",
	"INFO",
	"DEBUG",
	"TRACE",
	"(unknown)",
	NULL
};

static
void
logger_default_console_(void *data, enum logger_level level, const char *file, const char *function, int line, const char *fmt, va_list ap)
{
	struct logger_default_console_data *ld = (struct logger_default_console_data *)data;
	struct timeval now;

	FILE *f = (ld && ld->f) ? ld->f : stderr;

	gettimeofday(&now, NULL);

	flockfile(f);
	fprintf(f, "%lld.%06lld %s [%s:%s:%d] ",
	        (long long)now.tv_sec, (long long)now.tv_usec,
	        logger_level_names[level <= L_END_ ? level : L_END_],
	        file, function, line);

	vfprintf(f, fmt, ap);
	fputs("\n", f);
	fflush(f);
	funlockfile(f);
}

void
logger_add_handler(enum logger_level level, logger_vhandler_fn *fn, void *data, logger_vhandler_fini_fn *fini_fn)
{
	struct logger_handler_head *lhh;
	struct logger_handler *lh;

	if (level < L_CRITICAL || level >= L_END_) {
		return;
	}

	lhh = &logger_handlers[level];

	lh = malloc(sizeof *lh);
	if (lh == NULL) {
		perror("malloc");
		return;
	}
	lh->fn = fn;
	lh->data = data;
	lh->fini_fn = fini_fn;

	SLIST_INSERT_HEAD(lhh, lh, next);
}

void
logger_init(void)
{
	struct logger_handler_head *lhh;
	enum logger_level l;

	for (l = L_CRITICAL; l < L_END_; l++) {
		lhh = &logger_handlers[l];
		SLIST_INIT(lhh);
	}
}

void
logger_init_default(int level)
{
	logger_init();

	if (level >= L_END_) {
		level = L_END_ - 1;
	}

	for (/**/; level >= L_CRITICAL; level--) {
		logger_add_handler(level, logger_default_console, NULL, NULL);
	}
}

void
logger_fini(void)
{
	enum logger_level l;
	struct logger_handler_head *lhh;
	struct logger_handler *lh;

	for (l = L_CRITICAL; l < L_END_; l++) {
		lhh = &logger_handlers[l];
		while (! SLIST_EMPTY(lhh)) {
			lh = SLIST_FIRST(lhh);
			SLIST_REMOVE_HEAD(lhh, next);
			if (lh->fini_fn) {
				lh->fini_fn(lh->data);
			}
			free(lh);
		}
	}
}

void
logger(enum logger_level level, const char *file, const char *function, int line, const char *fmt, ...)
{
	va_list ap;
	struct logger_handler_head *lhh;
	struct logger_handler *lh;

	lhh = &logger_handlers[level];

	SLIST_FOREACH(lh, lhh, next) {
		va_start(ap, fmt);
		lh->fn(lh->data, level, file, function, line, fmt, ap);
		va_end(ap);
	}
}

#ifdef TEST
int
main(int argc, char **argv)
{
	(void)argc, (void)argv;

	logger_init_default(L_INFO);

	LOG_CRITICAL("my first log");
	LOG_INFO("check %s %d", "check", 123);
	LOG_DEBUG("should not see this");

	logger_fini();
}
#endif /* TEST */
