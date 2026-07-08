#include "core/ringbuf.h"

#include <string.h>

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t cap)
{
    rb->buf = storage;
    rb->cap = cap;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

void ringbuf_reset(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

size_t ringbuf_capacity(const ringbuf_t *rb) { return rb->cap; }
size_t ringbuf_size(const ringbuf_t *rb)     { return rb->count; }
size_t ringbuf_free(const ringbuf_t *rb)     { return rb->cap - rb->count; }
bool   ringbuf_is_empty(const ringbuf_t *rb) { return rb->count == 0; }
bool   ringbuf_is_full(const ringbuf_t *rb)  { return rb->count == rb->cap; }

static void rb_put(ringbuf_t *rb, const uint8_t *src, size_t n)
{
    /* Precondition: n <= free space. Handles wrap in up to two memcpys. */
    size_t first = rb->cap - rb->tail;
    if (first > n) {
        first = n;
    }
    memcpy(rb->buf + rb->tail, src, first);
    memcpy(rb->buf, src + first, n - first);
    rb->tail = (rb->tail + n) % rb->cap;
    rb->count += n;
}

static void rb_drop(ringbuf_t *rb, size_t n)
{
    /* Precondition: n <= count. */
    rb->head = (rb->head + n) % rb->cap;
    rb->count -= n;
}

size_t ringbuf_write(ringbuf_t *rb, const void *data, size_t len)
{
    if (rb->cap == 0 || len == 0) {
        return 0;
    }
    size_t space = rb->cap - rb->count;
    size_t n = (len < space) ? len : space;
    if (n > 0) {
        rb_put(rb, (const uint8_t *)data, n);
    }
    return n;
}

size_t ringbuf_write_overwrite(ringbuf_t *rb, const void *data, size_t len)
{
    if (rb->cap == 0 || len == 0) {
        return 0;
    }
    const uint8_t *src = (const uint8_t *)data;
    size_t dropped = 0;

    /* If the payload is larger than capacity, keep only the last cap bytes. */
    if (len >= rb->cap) {
        dropped = rb->count;         /* everything currently stored is discarded */
        ringbuf_reset(rb);
        src += (len - rb->cap);
        rb_put(rb, src, rb->cap);
        return dropped + (len - rb->cap);
    }

    size_t space = rb->cap - rb->count;
    if (len > space) {
        size_t need = len - space;
        rb_drop(rb, need);
        dropped = need;
    }
    rb_put(rb, src, len);
    return dropped;
}

size_t ringbuf_read(ringbuf_t *rb, void *out, size_t len)
{
    if (rb->cap == 0 || len == 0 || rb->count == 0) {
        return 0;
    }
    size_t n = (len < rb->count) ? len : rb->count;
    uint8_t *dst = (uint8_t *)out;
    size_t first = rb->cap - rb->head;
    if (first > n) {
        first = n;
    }
    memcpy(dst, rb->buf + rb->head, first);
    memcpy(dst + first, rb->buf, n - first);
    rb_drop(rb, n);
    return n;
}

size_t ringbuf_peek(const ringbuf_t *rb, void *out, size_t len)
{
    if (rb->cap == 0 || len == 0 || rb->count == 0) {
        return 0;
    }
    size_t n = (len < rb->count) ? len : rb->count;
    uint8_t *dst = (uint8_t *)out;
    size_t first = rb->cap - rb->head;
    if (first > n) {
        first = n;
    }
    memcpy(dst, rb->buf + rb->head, first);
    memcpy(dst + first, rb->buf, n - first);
    return n;
}
