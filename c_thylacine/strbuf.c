#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <gc.h>

#include "logger.h"
#include "strbuf.h"
#include "e.h"

static const size_t chunk_sz_ = 16;

/**
 * Round requested growth up to next chunk size.
 */
static
inline
size_t
buf_next_chunk_sz_(size_t need)
{
	size_t rem = need % chunk_sz_;
	if (rem) {
		need += chunk_sz_ - rem;
	}
	return need;
}

int
strbuf_appendf(char **buf, size_t *buf_alloc, size_t *buf_used,  const char *fmt, ...)
{
	va_list ap;
	int r = 0;

	assert(buf != NULL);
	assert(buf_alloc != NULL);
	assert(buf_used != NULL);
	assert(*buf != NULL || *buf_alloc == 0);

	do {
		size_t buf_free = *buf_alloc - *buf_used;
		char * buf_offset = *buf + *buf_used;
		va_start(ap, fmt);
		r = vsnprintf(buf_offset, buf_free, fmt, ap);
		va_end(ap);
		if (r < 0) {
			LOG_ERROR("%s: %s", "vsnprintf", strerror(errno));
			E_THROW(E_RESOURCE, NULL);
		}
		if (r >= (int)buf_free) {
			size_t buf_need = 1 + (size_t)r;
			size_t buf_grow = buf_next_chunk_sz_(buf_need);
			size_t new_alloc = *buf_alloc + buf_grow;
			void *tmpptr = GC_REALLOC(*buf, new_alloc);
			if (tmpptr == NULL) {
				LOG_ERROR("%s(%p, %zu): %s", "realloc", (void *)*buf, new_alloc, strerror(errno));
				E_THROW(E_RESOURCE, NULL);
			}
			*buf = tmpptr;
			*buf_alloc = new_alloc;
			continue;
		}
		*buf_used += r;
		break;
	}
	while (1);

	return r;
}
