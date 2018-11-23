#ifndef _E_H_
#define _E_H_

#define E_VERSION_MAJOR 1
#define E_VERSION_MINOR 2
#define E_VERSION_PATCH 0
#define E_BUILD 20181026000000ULL

#include <setjmp.h>
#include <sys/queue.h>
#include <stdio.h>

#ifndef CLANG_ANALYZER_NORETURN
#	ifdef __clang__
#		if __has_feature(attribute_analyzer_noreturn)
#			define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
#		else
#			define CLANG_ANALYZER_NORETURN
#		endif
#	else
#		define CLANG_ANALYZER_NORETURN
#	endif
#endif

struct e_frame {
	jmp_buf ctx;
	struct e_where {
		const char *file;
		const char *func;
		unsigned int line;
	} framed, thrown;
	SLIST_ENTRY(e_frame) next;
	void *data;
	int e;
	unsigned int finalized : 1;
	unsigned int handled : 1;
};

enum e_cases {
	E_FINALLY = -1,
	E_TRY = 0,
	/* actual exceptions shall be defined as positive integers */
#	include "exceptions.list"
};

void e_frame_start(struct e_frame *, const char *, const char *, unsigned int);
void e_frame_done(void);
void e_unhandled(void);
void e_throw(int, void *, const char *, const char *, unsigned int) CLANG_ANALYZER_NORETURN;
void e_throw_up(int, void *, const char *, const char *, unsigned int) CLANG_ANALYZER_NORETURN;
struct e_frame *e_current_frame(void);
void *e_data(void);
void e_frame_dumpf(FILE *, struct e_frame *);


typedef void (e_unhandled_handler_fn)(int, void *, const char *, const char *, unsigned int);
e_unhandled_handler_fn *e_unhandled_set(e_unhandled_handler_fn *);

#define E_START do {\
	int __e;\
	struct e_frame __ef;\
	e_frame_start(&__ef, __FILE__, __func__, __LINE__);\
	switch ((__e = setjmp(__ef.ctx))) {\
		default: if (__e != E_FINALLY) { e_unhandled(); } break;

#define E_DONE }\
	e_frame_done();\
} while (0);

#define E_THROW(__e__,__data__) e_throw((__e__), (__data__), __FILE__, __func__, __LINE__)

#define E_THROW_UP(__e__,__data__) e_throw_up((__e__), (__data__), __FILE__, __func__, __LINE__)

#define E_DATA e_data()

#endif /* _E_H_ */
