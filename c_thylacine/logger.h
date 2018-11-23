#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdarg.h>
#include <stdio.h>

enum logger_level {
	L_CRITICAL = 0,
	L_ERROR,
	L_WARNING,
	L_INFO,
	L_DEBUG,
	L_TRACE,
	L_END_
};

typedef void (logger_vhandler_fn)(void *, enum logger_level, const char *, const char *, int, const char *, va_list);
typedef void (logger_vhandler_fini_fn)(void *);

extern logger_vhandler_fn *logger_default_console;

struct logger_default_console_data {
	FILE *f;
};

void logger_init(void);
void logger_init_default(int);
void logger_fini(void);
void logger_add_handler(enum logger_level, logger_vhandler_fn *, void *, logger_vhandler_fini_fn *);
void logger(enum logger_level, const char *, const char *, int, const char *, ...);

#define LOGGER(__level__, ...) logger(__level__, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(...) do { LOGGER(L_CRITICAL, __VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { LOGGER(L_ERROR, __VA_ARGS__); } while (0)
#define LOG_WARNING(...) do { LOGGER(L_WARNING, __VA_ARGS__); } while (0)
#define LOG_INFO(...) do { LOGGER(L_INFO, __VA_ARGS__); } while (0)
#define LOG_DEBUG(...) do { LOGGER(L_DEBUG, __VA_ARGS__); } while (0)
#define LOG_TRACE(...) do { LOGGER(L_TRACE, __VA_ARGS__); } while (0)

#endif /* _LOGGER_H_ */
