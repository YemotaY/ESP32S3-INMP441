#include "net_socket.h"

#include <errno.h>
#include <string.h>

#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_log.h"

static const char *TAG = "net_socket";

int ss_tcp_connect(const char *host, int port)
{
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        /* Not a dotted-quad: resolve the hostname. */
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) {
            ESP_LOGE(TAG, "cannot resolve %s", host);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], sizeof(addr.sin_addr));
    }

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        ESP_LOGE(TAG, "socket: %d", errno);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ESP_LOGE(TAG, "connect %s:%d: %d", host, port, errno);
        close(fd);
        return -1;
    }
    /* Short recv timeout: recv() returns EWOULDBLOCK -> STREAM_WOULDBLOCK so the client
     * keeps streaming, and the post-speech drain waits (not busy-spins) for the cut. */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 2000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ESP_LOGI(TAG, "connected %s:%d (fd=%d)", host, port, fd);
    return fd;
}

static long tr_send(void *user, const void *buf, size_t len)
{
    tcp_transport_ctx_t *c = user;
    const uint8_t *p = buf;
    size_t sent = 0;
    while (sent < len) {
        int n = send(c->fd, p + sent, len - sent, 0);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR)) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }
    return (long)len;
}

static long tr_recv(void *user, void *buf, size_t len)
{
    tcp_transport_ctx_t *c = user;
    int n = recv(c->fd, buf, len, 0);
    if (n > 0) {
        return n;
    }
    if (n == 0) {
        return 0;   /* peer closed: server cut the connection */
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        return STREAM_WOULDBLOCK;
    }
    return -1;
}

stream_transport_t ss_tcp_transport(tcp_transport_ctx_t *ctx)
{
    stream_transport_t tr = { .send = tr_send, .recv = tr_recv, .user = ctx };
    return tr;
}

void ss_tcp_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
