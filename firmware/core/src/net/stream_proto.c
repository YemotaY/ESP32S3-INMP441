#include "core/net/stream_proto.h"

#include <string.h>

/* Count hex digits needed to represent `n` (n > 0). */
static size_t hex_digits(size_t n)
{
    size_t d = 0;
    do {
        d++;
        n >>= 4;
    } while (n);
    return d;
}

/* Write `n` as lowercase hex (no prefix) to `out`; returns digits written. */
static size_t write_hex(char *out, size_t n)
{
    static const char digits[] = "0123456789abcdef";
    size_t d = hex_digits(n);
    for (size_t i = 0; i < d; i++) {
        out[d - 1 - i] = digits[n & 0xF];
        n >>= 4;
    }
    return d;
}

/* Append a NUL-terminated string, advancing *pos; returns 0 on overflow. */
static int append(char *buf, size_t cap, size_t *pos, const char *s)
{
    size_t len = strlen(s);
    if (*pos + len > cap) {
        return 0;
    }
    memcpy(buf + *pos, s, len);
    *pos += len;
    return 1;
}

/* Append an unsigned decimal, advancing *pos; returns 0 on overflow. */
static int append_u32(char *buf, size_t cap, size_t *pos, uint32_t v)
{
    char tmp[10];
    size_t n = 0;
    if (v == 0) {
        tmp[n++] = '0';
    } else {
        char rev[10];
        size_t r = 0;
        while (v) {
            rev[r++] = (char)('0' + (v % 10));
            v /= 10;
        }
        while (r) {
            tmp[n++] = rev[--r];
        }
    }
    if (*pos + n > cap) {
        return 0;
    }
    memcpy(buf + *pos, tmp, n);
    *pos += n;
    return 1;
}

size_t stream_proto_build_request_header(char *buf, size_t cap,
                                         const stream_proto_cfg_t *cfg)
{
    if (!buf || !cfg || !cfg->path || !cfg->host || !cfg->session_id) {
        return 0;
    }
    size_t pos = 0;
    int ok = 1;
    ok &= append(buf, cap, &pos, "POST ");
    ok &= append(buf, cap, &pos, cfg->path);
    ok &= append(buf, cap, &pos, " HTTP/1.1\r\nHost: ");
    ok &= append(buf, cap, &pos, cfg->host);
    ok &= append(buf, cap, &pos,
                 "\r\nTransfer-Encoding: chunked\r\n"
                 "Content-Type: application/octet-stream\r\n"
                 "X-SimonSays-Session: ");
    ok &= append(buf, cap, &pos, cfg->session_id);
    ok &= append(buf, cap, &pos, "\r\nX-SimonSays-Wake-Conf: ");
    ok &= append_u32(buf, cap, &pos, cfg->wake_conf_milli);
    ok &= append(buf, cap, &pos, "\r\n\r\n");
    return ok ? pos : 0;
}

size_t stream_proto_chunk_overhead(size_t payload_len)
{
    if (payload_len == 0) {
        return 0;
    }
    /* hexlen + CRLF + payload + CRLF */
    return hex_digits(payload_len) + 2 + payload_len + 2;
}

size_t stream_proto_encode_chunk(char *buf, size_t cap,
                                 const void *payload, size_t payload_len)
{
    if (!buf || payload_len == 0) {
        return 0;
    }
    size_t need = stream_proto_chunk_overhead(payload_len);
    if (need > cap) {
        return 0;
    }
    size_t pos = 0;
    pos += write_hex(buf + pos, payload_len);
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    memcpy(buf + pos, payload, payload_len);
    pos += payload_len;
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    return pos;
}

size_t stream_proto_encode_final(char *buf, size_t cap)
{
    static const char term[] = "0\r\n\r\n";
    size_t n = sizeof(term) - 1;
    if (!buf || n > cap) {
        return 0;
    }
    memcpy(buf, term, n);
    return n;
}
