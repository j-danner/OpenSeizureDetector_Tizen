#ifndef INCLUDED_RINGBUF_H
#define INCLUDED_RINGBUF_H

/*
 * Based on 'https://github.com/dhess/c-ringbuf' (CC0)
 */

#include <dlog.h>



typedef struct ringbuf_t *ringbuf_t;

struct ringbuf_t
{
	int idx;
	int size;
	double* buf;
};


/*
 * Create a new ring buffer with the given capacity (usable
 * bytes). Note that the actual internal buffer size may be one or
 * more bytes larger than the usable capacity, for bookkeeping.
 *
 * Returns the new ring buffer object, or 0 if there's not enough
 * memory to fulfill the request for the given capacity.
 */
ringbuf_t ringbuf_new(int capacity);


/*
 * Deallocate a ring buffer, and, as a side effect, set the pointer to
 * 0.
 */
void ringbuf_free(ringbuf_t *rb);


/*
 * Push double val onto ring buffer rb.
 */
void ringbuf_push(ringbuf_t rb, double val);


/*
 * Get correctly rotated buffer and write it to dst
 */
void ringbuf_get_buf(ringbuf_t rb, double* dst);

/*
 * print buffer
 */
void ringbuf_print(ringbuf_t rb, const char *tag);

#endif /* INCLUDED_RINGBUF_H */
