#include "posix_transport.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int posix_connect(const char *host_ip, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host_ip, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    /* Short recv timeout so poll_close waits briefly instead of busy-spinning. */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 10000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return fd;
}

static long tr_send(void *user, const void *buf, size_t len)
{
    posix_transport_ctx_t *c = user;
    const uint8_t *p = buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(c->fd, p + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return (long)sent;
}

static long tr_recv(void *user, void *buf, size_t len)
{
    posix_transport_ctx_t *c = user;
    ssize_t n = recv(c->fd, buf, len, 0);
    if (n > 0) {
        return (long)n;
    }
    if (n == 0) {
        return 0;   /* peer closed */
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return STREAM_WOULDBLOCK;   /* recv timeout: no data yet */
    }
    return -1;
}

stream_transport_t posix_transport(posix_transport_ctx_t *ctx)
{
    stream_transport_t t = { .send = tr_send, .recv = tr_recv, .user = ctx };
    return t;
}

static size_t buf_read(void *user, void *buf, size_t cap)
{
    pcm_buf_src_t *s = user;
    size_t remain = s->len - s->pos;
    size_t take = remain < cap ? remain : cap;
    if (take > 0) {
        memcpy(buf, s->data + s->pos, take);
        s->pos += take;
    }
    return take;   /* 0 => end of audio */
}

pcm_source_t pcm_buffer_source(pcm_buf_src_t *s)
{
    pcm_source_t src = { .read = buf_read, .user = s };
    return src;
}
