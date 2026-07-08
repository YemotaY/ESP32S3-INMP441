#include "server/http_ingest.h"
#include "core/net/stream_proto.h"
#include "test.h"

#include <string.h>

typedef struct {
    uint8_t buf[4096];
    size_t  len;
} capture_t;

static void on_payload(void *user, const uint8_t *data, size_t len)
{
    capture_t *c = user;
    if (c->len + len <= sizeof(c->buf)) {
        memcpy(c->buf + c->len, data, len);
        c->len += len;
    }
}

/* Build a full chunked POST into `out`, returning its length. */
static size_t build_request(uint8_t *out, size_t cap,
                            const uint8_t *pcm, size_t pcm_len, size_t frame)
{
    stream_proto_cfg_t cfg = {
        .path = "/v1/stream", .host = "host:8080",
        .session_id = "abc123", .wake_conf_milli = 875,
    };
    size_t pos = stream_proto_build_request_header((char *)out, cap, &cfg);
    for (size_t off = 0; off < pcm_len; off += frame) {
        size_t n = pcm_len - off;
        if (n > frame) {
            n = frame;
        }
        pos += stream_proto_encode_chunk((char *)out + pos, cap - pos, pcm + off, n);
    }
    pos += stream_proto_encode_final((char *)out + pos, cap - pos);
    return pos;
}

static void test_full_parse(void)
{
    uint8_t pcm[100];
    for (size_t i = 0; i < sizeof(pcm); i++) {
        pcm[i] = (uint8_t)(i * 7 + 1);
    }
    uint8_t req[2048];
    size_t rlen = build_request(req, sizeof(req), pcm, sizeof(pcm), 30);

    capture_t cap = {0};
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &cap);
    long consumed = http_ingest_feed(&ing, req, rlen);

    CHECK_EQ_INT(consumed, (long)rlen);
    CHECK(ing.complete);
    CHECK_STR_EQ(ing.method, "POST");
    CHECK_STR_EQ(ing.path, "/v1/stream");
    CHECK_STR_EQ(ing.session_id, "abc123");
    CHECK_EQ_INT(ing.wake_conf_milli, 875);
    CHECK_EQ_INT(cap.len, sizeof(pcm));
    CHECK(memcmp(cap.buf, pcm, sizeof(pcm)) == 0);
}

static void test_incremental_bytewise(void)
{
    uint8_t pcm[64];
    for (size_t i = 0; i < sizeof(pcm); i++) {
        pcm[i] = (uint8_t)(255 - i);
    }
    uint8_t req[2048];
    size_t rlen = build_request(req, sizeof(req), pcm, sizeof(pcm), 16);

    capture_t cap = {0};
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &cap);
    for (size_t i = 0; i < rlen; i++) {
        long r = http_ingest_feed(&ing, req + i, 1);
        CHECK(r == 1);
    }
    CHECK(ing.complete);
    CHECK_EQ_INT(cap.len, sizeof(pcm));
    CHECK(memcmp(cap.buf, pcm, sizeof(pcm)) == 0);
}

static void test_malformed_chunk(void)
{
    const char *bad =
        "POST /x HTTP/1.1\r\nHost: h\r\nTransfer-Encoding: chunked\r\n\r\n"
        "zz\r\n";   /* "zz" is not a valid hex chunk size */
    capture_t cap = {0};
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &cap);
    long r = http_ingest_feed(&ing, (const uint8_t *)bad, strlen(bad));
    CHECK_EQ_INT(r, -1);
    CHECK(!ing.complete);
}

static void test_split_across_feeds(void)
{
    uint8_t pcm[40];
    memset(pcm, 0x55, sizeof(pcm));
    uint8_t req[1024];
    size_t rlen = build_request(req, sizeof(req), pcm, sizeof(pcm), 40);

    capture_t cap = {0};
    http_ingest_t ing;
    http_ingest_init(&ing, on_payload, NULL, &cap);
    /* Feed in two arbitrary halves. */
    size_t half = rlen / 2;
    http_ingest_feed(&ing, req, half);
    http_ingest_feed(&ing, req + half, rlen - half);
    CHECK(ing.complete);
    CHECK_EQ_INT(cap.len, sizeof(pcm));
    CHECK(memcmp(cap.buf, pcm, sizeof(pcm)) == 0);
}

TEST_MAIN_BEGIN("http_ingest")
    RUN(test_full_parse);
    RUN(test_incremental_bytewise);
    RUN(test_malformed_chunk);
    RUN(test_split_across_feeds);
TEST_MAIN_END()
