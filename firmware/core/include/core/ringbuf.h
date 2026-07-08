/* Portable FIFO byte ring buffer over caller-provided storage.
 * No dynamic allocation; suitable for audio PCM buffering on device or host.
 */
#ifndef CORE_RINGBUF_H
#define CORE_RINGBUF_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;   /* caller-owned storage */
    size_t   cap;   /* capacity in bytes */
    size_t   head;  /* read index */
    size_t   tail;  /* write index */
    size_t   count; /* bytes currently stored */
} ringbuf_t;

/* Initialize over `storage` of `cap` bytes. `storage` must outlive the ringbuf. */
void   ringbuf_init(ringbuf_t *rb, uint8_t *storage, size_t cap);
void   ringbuf_reset(ringbuf_t *rb);

size_t ringbuf_capacity(const ringbuf_t *rb);
size_t ringbuf_size(const ringbuf_t *rb);  /* bytes stored */
size_t ringbuf_free(const ringbuf_t *rb);  /* bytes free */
bool   ringbuf_is_empty(const ringbuf_t *rb);
bool   ringbuf_is_full(const ringbuf_t *rb);

/* Write up to `len` bytes without overwriting. Returns bytes actually written. */
size_t ringbuf_write(ringbuf_t *rb, const void *data, size_t len);

/* Write `len` bytes, overwriting the oldest data if needed.
 * Returns the number of bytes discarded from the front to make room. */
size_t ringbuf_write_overwrite(ringbuf_t *rb, const void *data, size_t len);

/* Read (consume) up to `len` bytes into `out`. Returns bytes read. */
size_t ringbuf_read(ringbuf_t *rb, void *out, size_t len);

/* Copy up to `len` bytes into `out` without consuming. Returns bytes copied. */
size_t ringbuf_peek(const ringbuf_t *rb, void *out, size_t len);

#endif /* CORE_RINGBUF_H */
