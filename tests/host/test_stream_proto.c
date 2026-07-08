#include "core/net/stream_proto.h"
#include "test.h"

#include <string.h>

static void test_request_header(void)
{
    char buf[512];
    stream_proto_cfg_t cfg = {
        .path = "/v1/stream", .host = "10.0.0.5:8080",
        .session_id = "sess42", .wake_conf_milli = 950,
    };
    size_t n = stream_proto_build_request_header(buf, sizeof(buf), &cfg);
    CHECK(n > 0);
    buf[n] = '\0';
    CHECK(strncmp(buf, "POST /v1/stream HTTP/1.1\r\n", 25) == 0);
    CHECK(strstr(buf, "Host: 10.0.0.5:8080\r\n") != NULL);
    CHECK(strstr(buf, "Transfer-Encoding: chunked\r\n") != NULL);
    CHECK(strstr(buf, "X-SimonSays-Session: sess42\r\n") != NULL);
    CHECK(strstr(buf, "X-SimonSays-Wake-Conf: 950\r\n") != NULL);
    /* header terminates with a blank line */
    CHECK(strcmp(buf + n - 4, "\r\n\r\n") == 0);
}

static void test_header_too_small(void)
{
    char buf[16];
    stream_proto_cfg_t cfg = {
        .path = "/v1/stream", .host = "h", .session_id = "s", .wake_conf_milli = 1,
    };
    CHECK_EQ_INT(stream_proto_build_request_header(buf, sizeof(buf), &cfg), 0);
}

static void test_encode_chunk(void)
{
    char buf[32];
    const uint8_t payload[3] = { 0x01, 0x02, 0x03 };
    size_t n = stream_proto_encode_chunk(buf, sizeof(buf), payload, 3);
    CHECK_EQ_INT(n, 8);                       /* "3\r\n" + 3 bytes + "\r\n" */
    CHECK_EQ_INT(stream_proto_chunk_overhead(3), 8);
    CHECK(buf[0] == '3');
    CHECK(buf[1] == '\r' && buf[2] == '\n');
    CHECK(buf[3] == 0x01 && buf[4] == 0x02 && buf[5] == 0x03);
    CHECK(buf[6] == '\r' && buf[7] == '\n');
}

static void test_encode_chunk_hexlen(void)
{
    char buf[300];
    uint8_t payload[255];
    memset(payload, 0xAB, sizeof(payload));
    size_t n = stream_proto_encode_chunk(buf, sizeof(buf), payload, 255);
    CHECK_EQ_INT(n, stream_proto_chunk_overhead(255));  /* "ff"=2 + 2 + 255 + 2 */
    CHECK(buf[0] == 'f' && buf[1] == 'f');
    CHECK(buf[2] == '\r' && buf[3] == '\n');
}

static void test_encode_chunk_rejects_empty(void)
{
    char buf[16];
    CHECK_EQ_INT(stream_proto_encode_chunk(buf, sizeof(buf), "x", 0), 0);
}

static void test_encode_chunk_too_small(void)
{
    char buf[4];
    const uint8_t payload[3] = { 1, 2, 3 };
    CHECK_EQ_INT(stream_proto_encode_chunk(buf, sizeof(buf), payload, 3), 0);
}

static void test_encode_final(void)
{
    char buf[8];
    size_t n = stream_proto_encode_final(buf, sizeof(buf));
    CHECK_EQ_INT(n, 5);
    CHECK(memcmp(buf, "0\r\n\r\n", 5) == 0);
}

TEST_MAIN_BEGIN("stream_proto")
    RUN(test_request_header);
    RUN(test_header_too_small);
    RUN(test_encode_chunk);
    RUN(test_encode_chunk_hexlen);
    RUN(test_encode_chunk_rejects_empty);
    RUN(test_encode_chunk_too_small);
    RUN(test_encode_final);
TEST_MAIN_END()
