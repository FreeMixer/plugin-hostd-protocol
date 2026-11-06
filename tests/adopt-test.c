/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * socket_adopt: a socket the caller bound is served as socket_start serves its own, and the port stays the caller's: a
 * second bind on it is refused (EADDRINUSE), which a bind 0, close and rebind does not guarantee. An fd that is no
 * bound TCP socket is refused, nothing served.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */
#include "../src/socket.c"

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

void effects_idle_external_uis(void)
{
}

static volatile int g_running = 1;
static int g_failures;

static void echo_cb(msg_t *msg)
{
    char reply[64];
    snprintf(reply, sizeof(reply), "got %s", msg->data);
    socket_send(msg->sender_id, reply, strlen(reply) + 1);
}

static void *server_thread(void *arg)
{
    (void)arg;
    while (g_running)
        socket_run(0);
    return NULL;
}

/* a socket bound to the loopback at port_or_0; reuse sets SO_REUSEPORT first, as socket_start does */
static int bound(uint16_t port_or_0, uint16_t *port, int reuse)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = socket(AF_INET, SOCK_STREAM, 0), one = 1;

    if (fd >= 0 && reuse)
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port_or_0);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        if (fd >= 0)
            close(fd);
        return -1;
    }
    getsockname(fd, (struct sockaddr *)&addr, &len);
    if (port)
        *port = ntohs(addr.sin_port);
    return fd;
}

static void check(int cond, const char *what)
{
    printf("%s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond)
        g_failures++;
}

int main(void)
{
    uint16_t port = 0, fbport = 0;
    struct sockaddr_in addr;
    struct timeval tv = { 2, 0 };
    char buf[64];
    pthread_t thread;
    int fd, fb, c, n = 0, again;

    signal(SIGPIPE, SIG_IGN);
    check(socket_adopt(-1, -1, 4096) == -1, "an fd that is not open is refused");
    int unbound = socket(AF_INET, SOCK_STREAM, 0);
    check(socket_adopt(unbound, -1, 4096) == -1, "a socket bound to nothing is refused");
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    check(socket_adopt(udp, -1, 4096) == -1, "a datagram socket is refused");
    close(unbound);
    close(udp);

    fd = bound(0, &port, 0);
    fb = bound(0, &fbport, 0);
    check(fd >= 0 && fb >= 0, "the caller binds two ports");
    check(socket_adopt(fd, fb, 4096) == 0, "socket_adopt serves the caller's sockets");
    socket_set_receive_cb(echo_cb);
    pthread_create(&thread, NULL, server_thread, NULL);

    /* the port stays the caller's: no second bind takes it, not even one that asks SO_REUSEPORT as socket_start does */
    again = bound(port, NULL, 1);
    check(again < 0 && errno == EADDRINUSE, "a second bind on the adopted port is refused with EADDRINUSE");
    if (again >= 0)
        close(again);

    c = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    check(connect(c, (struct sockaddr *)&addr, sizeof(addr)) == 0, "a client reaches the adopted port");
    /* the controller connects to the feedback port too, as mod-host's server waits for both */
    int cf = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_port = htons(fbport);
    check(connect(cf, (struct sockaddr *)&addr, sizeof(addr)) == 0, "a client reaches the adopted feedback port");
    send(c, "ping", 5, MSG_NOSIGNAL);
    while (n < (int)sizeof(buf) - 1 && recv(c, buf + n, 1, 0) == 1 && buf[n] != '\0')
        n++;
    buf[n] = '\0';
    check(strcmp(buf, "got ping") == 0, "a line sent to the adopted port is answered");
    close(c);
    close(cf);

    g_running = 0;
    socket_finish();
    pthread_join(thread, NULL);
    printf("%s\n", g_failures ? "adopt test FAILED" : "adopt test ok");
    return g_failures ? 1 : 0;
}
