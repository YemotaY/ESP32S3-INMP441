#include "core/ringbuf.h"
#include "test.h"

static void test_init_empty(void)
{
    uint8_t store[8];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);
    CHECK_EQ_INT(ringbuf_capacity(&rb), 8);
    CHECK_EQ_INT(ringbuf_size(&rb), 0);
    CHECK_EQ_INT(ringbuf_free(&rb), 8);
    CHECK(ringbuf_is_empty(&rb));
    CHECK(!ringbuf_is_full(&rb));
}

static void test_write_read_roundtrip(void)
{
    uint8_t store[8];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    const uint8_t in[5] = {1, 2, 3, 4, 5};
    CHECK_EQ_INT(ringbuf_write(&rb, in, 5), 5);
    CHECK_EQ_INT(ringbuf_size(&rb), 5);
    CHECK_EQ_INT(ringbuf_free(&rb), 3);

    uint8_t out[5] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 5), 5);
    CHECK_EQ_INT(memcmp(in, out, 5), 0);
    CHECK(ringbuf_is_empty(&rb));
}

static void test_write_full_no_overwrite(void)
{
    uint8_t store[4];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    const uint8_t in[6] = {1, 2, 3, 4, 5, 6};
    /* Only 4 fit; the rest is refused. */
    CHECK_EQ_INT(ringbuf_write(&rb, in, 6), 4);
    CHECK(ringbuf_is_full(&rb));
    CHECK_EQ_INT(ringbuf_write(&rb, in, 1), 0);

    uint8_t out[4] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 4), 4);
    CHECK_EQ_INT(memcmp(in, out, 4), 0);
}

static void test_wraparound(void)
{
    uint8_t store[4];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    uint8_t a[3] = {10, 11, 12};
    CHECK_EQ_INT(ringbuf_write(&rb, a, 3), 3);
    uint8_t tmp[2] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, tmp, 2), 2); /* head advances to 2 */
    CHECK_EQ_INT(tmp[0], 10);
    CHECK_EQ_INT(tmp[1], 11);

    uint8_t b[3] = {20, 21, 22};
    CHECK_EQ_INT(ringbuf_write(&rb, b, 3), 3); /* wraps around end of storage */
    CHECK_EQ_INT(ringbuf_size(&rb), 4);

    uint8_t out[4] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 4), 4);
    CHECK_EQ_INT(out[0], 12);
    CHECK_EQ_INT(out[1], 20);
    CHECK_EQ_INT(out[2], 21);
    CHECK_EQ_INT(out[3], 22);
}

static void test_overwrite_partial(void)
{
    uint8_t store[4];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    uint8_t a[4] = {1, 2, 3, 4};
    CHECK_EQ_INT(ringbuf_write(&rb, a, 4), 4);

    uint8_t b[2] = {5, 6};
    /* Overwrites the two oldest bytes (1,2). */
    CHECK_EQ_INT(ringbuf_write_overwrite(&rb, b, 2), 2);
    CHECK_EQ_INT(ringbuf_size(&rb), 4);

    uint8_t out[4] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 4), 4);
    CHECK_EQ_INT(out[0], 3);
    CHECK_EQ_INT(out[1], 4);
    CHECK_EQ_INT(out[2], 5);
    CHECK_EQ_INT(out[3], 6);
}

static void test_overwrite_larger_than_capacity(void)
{
    uint8_t store[4];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    uint8_t seed[2] = {1, 2};
    CHECK_EQ_INT(ringbuf_write(&rb, seed, 2), 2);

    uint8_t big[6] = {10, 11, 12, 13, 14, 15};
    /* Keeps only last 4 bytes; drops the 2 seeded + 2 leading of payload. */
    size_t dropped = ringbuf_write_overwrite(&rb, big, 6);
    CHECK_EQ_INT(dropped, 4);
    CHECK_EQ_INT(ringbuf_size(&rb), 4);

    uint8_t out[4] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 4), 4);
    CHECK_EQ_INT(out[0], 12);
    CHECK_EQ_INT(out[1], 13);
    CHECK_EQ_INT(out[2], 14);
    CHECK_EQ_INT(out[3], 15);
}

static void test_peek_does_not_consume(void)
{
    uint8_t store[8];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    uint8_t in[3] = {7, 8, 9};
    ringbuf_write(&rb, in, 3);

    uint8_t out[3] = {0};
    CHECK_EQ_INT(ringbuf_peek(&rb, out, 3), 3);
    CHECK_EQ_INT(memcmp(in, out, 3), 0);
    CHECK_EQ_INT(ringbuf_size(&rb), 3); /* still there */
}

static void test_partial_read(void)
{
    uint8_t store[8];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);

    uint8_t in[2] = {1, 2};
    ringbuf_write(&rb, in, 2);

    uint8_t out[5] = {0};
    CHECK_EQ_INT(ringbuf_read(&rb, out, 5), 2); /* only 2 available */
    CHECK(ringbuf_is_empty(&rb));
}

static void test_reset(void)
{
    uint8_t store[8];
    ringbuf_t rb;
    ringbuf_init(&rb, store, sizeof store);
    ringbuf_write(&rb, (const uint8_t[]){1, 2, 3}, 3);
    ringbuf_reset(&rb);
    CHECK(ringbuf_is_empty(&rb));
    CHECK_EQ_INT(ringbuf_size(&rb), 0);
}

TEST_MAIN_BEGIN("ringbuf")
    RUN(test_init_empty);
    RUN(test_write_read_roundtrip);
    RUN(test_write_full_no_overwrite);
    RUN(test_wraparound);
    RUN(test_overwrite_partial);
    RUN(test_overwrite_larger_than_capacity);
    RUN(test_peek_does_not_consume);
    RUN(test_partial_read);
    RUN(test_reset);
TEST_MAIN_END()
