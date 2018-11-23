#ifndef _ISOC11_SOURCE
#	define _ISOC11_SOURCE
#endif
#ifdef _THREAD_SAFE
#	define THREAD_LOCAL _Thread_local
#else
#	define THREAD_LOCAL
#endif

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <sys/queue.h>
#include <assert.h>

#include "e.h"

#ifndef E_DEBUGGING
#define E_DEBUG(...)
#else
#define E_DEBUG(...) do { FILE * f = stderr; fprintf(f, "%*sE_DEBUG: ", e_frame_depth_, ""); fprintf(f, __VA_ARGS__); fputc('\n', f); fflush(f); } while (0)
#endif

static THREAD_LOCAL SLIST_HEAD(e_frame_stack, e_frame) e_frame_stack_ = SLIST_HEAD_INITIALIZER(e_frame_stack_);
static THREAD_LOCAL int e_frame_depth_ = 0;

#ifdef GCOV
void __gcov_flush(void);
#endif

void
e_frame_dumpf(FILE *f, struct e_frame *ef)
{
	fprintf(f, "e_frame_stack %p:\n", (void *)ef);
	if (ef) {
		fprintf(f, "> block began @%s:%s:%u\n", ef->framed.file, ef->framed.func, ef->framed.line);
		fprintf(f, "> data: %p\n", ef->data);
		fprintf(f, "> e: %u finalized: %u handled: %u\n", ef->e, ef->finalized, ef->handled);
		fprintf(f, "> thrown from @%s:%s:%u\n", ef->thrown.file, ef->thrown.func, ef->thrown.line);
	}
	fputc('\n', f);
	fflush(f);
}

static
void
default_unhandled_handler_(int e, void *data, const char *file, const char *func, unsigned int line)
{
	fprintf(stderr, "unhandled exception %d (data: %p) thrown @%s:%s:%u\n",
	        e, data, file, func, line);
#ifdef GCOV
	__gcov_flush();
#endif
	abort();
}

static e_unhandled_handler_fn *unhandled_handler_ = default_unhandled_handler_;

e_unhandled_handler_fn *
e_unhandled_set(e_unhandled_handler_fn *fn)
{
	e_unhandled_handler_fn *old = unhandled_handler_;
	unhandled_handler_ = fn;
	return old;
}

void
e_frame_start(struct e_frame *ef, const char *file, const char *func, unsigned int line)
{
	assert(ef != NULL);

	memset(ef, 0, sizeof *ef);
	ef->framed.file = file;
	ef->framed.func = func;
	ef->framed.line = line;

	E_DEBUG("frame start @%s:%s:%u", ef->framed.file, ef->framed.func, ef->framed.line);

	SLIST_INSERT_HEAD(&e_frame_stack_, ef, next);
	e_frame_depth_++;
}

void
e_frame_done(void)
{
	struct e_frame *ef = SLIST_FIRST(&e_frame_stack_);

	assert(ef != NULL);

	if ( ! ef->finalized) {
		E_DEBUG("finalizing frame @%s:%s:%u", ef->framed.file, ef->framed.func, ef->framed.line);
		ef->finalized = 1;
		longjmp(ef->ctx, E_FINALLY);
	}

	if ( ! ef->handled && ef->e) {
		E_DEBUG("handling e:%u @%s:%s:%u", ef->e, ef->framed.file, ef->framed.func, ef->framed.line);
		ef->handled = 1;
		longjmp(ef->ctx, ef->e);
	}

	SLIST_REMOVE_HEAD(&e_frame_stack_, next);
	e_frame_depth_--;

	E_DEBUG("frame done @%s:%s:%u", ef->framed.file, ef->framed.func, ef->framed.line);
}

/* throw an exception within the TRY block of a frame */
void
e_throw(int e, void *data, const char *file, const char *func, unsigned int line)
{
	struct e_frame *ef = SLIST_FIRST(&e_frame_stack_);

	E_DEBUG("throw %d @%s:%s:%u", e, file, func, line);

	assert(ef != NULL);
	assert(e > E_TRY);

	ef->e = e;
	ef->data = data;
	ef->thrown.file = file;
	ef->thrown.func = func;
	ef->thrown.line = line;
	ef->finalized = 1;
	longjmp(ef->ctx, E_FINALLY);
}

struct e_frame *
e_current_frame(void) {
	return SLIST_FIRST(&e_frame_stack_);
}

void *
e_data(void)
{
	struct e_frame *ef = SLIST_FIRST(&e_frame_stack_);
	return ef ? ef->data : NULL;
}

/* throw an exception to the next frame, from within a non-TRY block handler */
void
e_throw_up(int e, void *data, const char *file, const char *func, unsigned int line)
{
	struct e_frame *ef = SLIST_FIRST(&e_frame_stack_);

	E_DEBUG("throw_up %d @%s:%s:%u", e, file, func, line);

	assert(ef != NULL);
	assert(e > E_TRY);

	SLIST_REMOVE_HEAD(&e_frame_stack_, next);
	e_frame_depth_--;

	if (SLIST_FIRST(&e_frame_stack_) == NULL) {
		if (unhandled_handler_ != NULL) {
			unhandled_handler_(ef->e, ef->data, ef->thrown.file, ef->thrown.func, ef->thrown.line);
		}
		return;
	}
	e_throw(e, data, file, func, line);
}

/* pass the existing exception to the next frame, when the existing frame does not handle it */
void
e_unhandled(void)
{
	struct e_frame *ef = SLIST_FIRST(&e_frame_stack_);

	assert(ef != NULL);

	E_DEBUG("%u unhandled in current stack", ef->e);
#ifdef E_DEBUGGING
	e_frame_dumpf(stderr, ef);
#endif

	/* try again, a step up */
	SLIST_REMOVE_HEAD(&e_frame_stack_, next);
	e_frame_depth_--;

	if (SLIST_FIRST(&e_frame_stack_) == NULL) {
		if (unhandled_handler_ != NULL) {
			unhandled_handler_(ef->e, ef->data, ef->thrown.file, ef->thrown.func, ef->thrown.line);
		}
		return;
	}
	e_throw(ef->e, ef->data, ef->thrown.file, ef->thrown.func, ef->thrown.line);
}
