#include "server/http_ingest.h"

#include <string.h>
#include <strings.h>

enum {
    ST_REQUEST_LINE = 0,
    ST_HEADER_LINE,
    ST_CHUNK_SIZE,
    ST_CHUNK_DATA,
    ST_CHUNK_CRLF,
    ST_TRAILER,
    ST_DONE,
    ST_ERROR,
};

void http_ingest_init(http_ingest_t *ing,
                      http_ingest_payload_cb on_payload,
                      http_ingest_header_cb on_header,
                      void *user)
{
    memset(ing, 0, sizeof(*ing));
    ing->state = ST_REQUEST_LINE;
    ing->on_payload = on_payload;
    ing->on_header = on_header;
    ing->user = user;
}

/* Accumulate bytes into ing->line until '\n'. On a complete line, strips a trailing
 * '\r', NUL-terminates, and returns 1 with *consumed advanced. Returns 0 if more input
 * is needed, -1 on line overflow. */
static int accumulate_line(http_ingest_t *ing, const uint8_t *buf, size_t len,
                           size_t *consumed)
{
    while (*consumed < len) {
        char c = (char)buf[(*consumed)++];
        if (c == '\n') {
            if (ing->line_len > 0 && ing->line[ing->line_len - 1] == '\r') {
                ing->line_len--;
            }
            ing->line[ing->line_len] = '\0';
            return 1;
        }
        if (ing->line_len >= HTTP_INGEST_LINE_MAX - 1) {
            return -1;
        }
        ing->line[ing->line_len++] = c;
    }
    return 0;
}

static void reset_line(http_ingest_t *ing)
{
    ing->line_len = 0;
}

static void copy_field(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static int parse_request_line(http_ingest_t *ing)
{
    /* "METHOD PATH HTTP/1.1" */
    const char *s = ing->line;
    const char *sp1 = strchr(s, ' ');
    if (!sp1) {
        return -1;
    }
    size_t mlen = (size_t)(sp1 - s);
    if (mlen == 0 || mlen >= sizeof(ing->method)) {
        return -1;
    }
    memcpy(ing->method, s, mlen);
    ing->method[mlen] = '\0';

    const char *pstart = sp1 + 1;
    const char *sp2 = strchr(pstart, ' ');
    if (!sp2) {
        return -1;
    }
    size_t plen = (size_t)(sp2 - pstart);
    if (plen == 0 || plen >= HTTP_INGEST_PATH_MAX) {
        return -1;
    }
    memcpy(ing->path, pstart, plen);
    ing->path[plen] = '\0';
    return 0;
}

static uint32_t parse_u32(const char *s)
{
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}

static void parse_header_line(http_ingest_t *ing)
{
    char *colon = strchr(ing->line, ':');
    if (!colon) {
        return;
    }
    *colon = '\0';
    char *name = ing->line;
    char *value = colon + 1;
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    if (strcasecmp(name, "X-SimonSays-Session") == 0) {
        copy_field(ing->session_id, sizeof(ing->session_id), value);
    } else if (strcasecmp(name, "X-SimonSays-Wake-Conf") == 0) {
        ing->wake_conf_milli = parse_u32(value);
    }
    if (ing->on_header) {
        ing->on_header(ing->user, name, value);
    }
}

/* Parse a hex chunk-size line (ignore any ";ext" suffix). Returns the size, or SIZE_MAX
 * on malformed input. */
static size_t parse_chunk_size(const char *s)
{
    size_t v = 0;
    int digits = 0;
    while (*s) {
        char c = *s;
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else if (c == ';') {
            break;              /* chunk extension: ignore */
        } else {
            return (size_t)-1;
        }
        v = (v << 4) | (size_t)d;
        digits++;
        s++;
    }
    return digits ? v : (size_t)-1;
}

long http_ingest_feed(http_ingest_t *ing, const uint8_t *buf, size_t len)
{
    if (ing->state == ST_ERROR) {
        return -1;
    }
    size_t pos = 0;
    while (pos < len && ing->state != ST_DONE) {
        switch (ing->state) {
        case ST_REQUEST_LINE: {
            int r = accumulate_line(ing, buf, len, &pos);
            if (r < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (r == 0) { return (long)pos; }
            if (parse_request_line(ing) < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            reset_line(ing);
            ing->state = ST_HEADER_LINE;
            break;
        }
        case ST_HEADER_LINE: {
            int r = accumulate_line(ing, buf, len, &pos);
            if (r < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (r == 0) { return (long)pos; }
            if (ing->line_len == 0) {
                reset_line(ing);
                ing->state = ST_CHUNK_SIZE;   /* blank line ends headers */
            } else {
                parse_header_line(ing);
                reset_line(ing);
            }
            break;
        }
        case ST_CHUNK_SIZE: {
            int r = accumulate_line(ing, buf, len, &pos);
            if (r < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (r == 0) { return (long)pos; }
            size_t sz = parse_chunk_size(ing->line);
            reset_line(ing);
            if (sz == (size_t)-1) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (sz == 0) {
                ing->state = ST_TRAILER;
            } else {
                ing->chunk_remaining = sz;
                ing->state = ST_CHUNK_DATA;
            }
            break;
        }
        case ST_CHUNK_DATA: {
            size_t avail = len - pos;
            size_t take = avail < ing->chunk_remaining ? avail : ing->chunk_remaining;
            if (take > 0 && ing->on_payload) {
                ing->on_payload(ing->user, buf + pos, take);
            }
            pos += take;
            ing->chunk_remaining -= take;
            if (ing->chunk_remaining == 0) {
                ing->state = ST_CHUNK_CRLF;
            }
            break;
        }
        case ST_CHUNK_CRLF: {
            int r = accumulate_line(ing, buf, len, &pos);
            if (r < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (r == 0) { return (long)pos; }
            if (ing->line_len != 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            reset_line(ing);
            ing->state = ST_CHUNK_SIZE;
            break;
        }
        case ST_TRAILER: {
            /* Consume the final CRLF after the 0-chunk (no trailer fields supported). */
            int r = accumulate_line(ing, buf, len, &pos);
            if (r < 0) { ing->state = ST_ERROR; ing->error = 1; return -1; }
            if (r == 0) { return (long)pos; }
            reset_line(ing);
            ing->complete = 1;
            ing->state = ST_DONE;
            break;
        }
        default:
            ing->state = ST_ERROR;
            ing->error = 1;
            return -1;
        }
    }
    return (long)pos;
}
