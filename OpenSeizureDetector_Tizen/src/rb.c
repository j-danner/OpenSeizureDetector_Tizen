/*
 * Based on 'https://github.com/dhess/c-ringbuf' (CC0)
  */


#include "rb.h"
#include <stdlib.h>


//create new ringbuf
ringbuf_t ringbuf_new(int capacity)
{
    ringbuf_t rb = malloc(sizeof(struct ringbuf_t));
    if (rb) {
        /* One byte is used for detecting the full condition. */
        rb->size = capacity;
        rb->buf = malloc(capacity * sizeof(double));
        rb->idx = 0;
        if (!rb->buf) {
        	free(rb);
        	return 0;
        }
    }
    return rb;
}


void ringbuf_free(ringbuf_t *rb)
{
    free((*rb)->buf);
    free(*rb);
    *rb = 0;
}

//push element to buffer
void ringbuf_push(ringbuf_t rb, double val)
{
	rb->buf[rb->idx] = val;
	rb->idx = (rb->idx+1) % (rb->size);
}

//get correctly rotated buffer and write it to dst
void ringbuf_get_buf(ringbuf_t rb, double* dst)
{
	for (int i = 0; i < rb->size; ++i) {
		dst[i] = (rb->buf)[(rb->idx + i) % rb->size];
	}
}


/* function to print an array */
void ringbuf_print(ringbuf_t rb, const char *tag)
{
    for (int i = 0; i < rb->size; i++)
        dlog_print(DLOG_INFO, tag, "%f ", rb->buf[i]);
}

