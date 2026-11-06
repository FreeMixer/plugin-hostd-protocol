/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol: a controller of the library's own server in a test, on a real TCP socket
 * (its command port and its feedback port), one NUL-terminated line at a time as mod-host frames it.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

/* the command port; the feedback port is the next one */
static int g_port;

static int client_connect(int port)
{
    struct sockaddr_in addr;
    struct timeval timeout = { 2, 0 };
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return fd;
}

typedef struct {
    int cmd, fb;
} conn_t;

static conn_t open_session(void)
{
    conn_t c;

    c.cmd = client_connect(g_port);
    c.fb = client_connect(g_port + 1);
    return c;
}

static void close_session(conn_t *c)
{
    close(c->cmd);
    close(c->fb);
    /* let socket_run() see the disconnection and wait for the next controller */
    usleep(100000);
}

/* one NUL-terminated line from fd, "" when none came in time */
static void read_line(int fd, char *buf, size_t size)
{
    size_t got = 0;

    while (got < size - 1)
    {
        ssize_t n = recv(fd, buf + got, 1, 0);
        if (n <= 0 || buf[got] == '\0')
            break;
        got++;
    }
    buf[got] = '\0';
}

static void exchange(conn_t *c, const char *command, char *reply, size_t size)
{
    send(c->cmd, command, strlen(command) + 1, MSG_NOSIGNAL);
    read_line(c->cmd, reply, size);
}

#endif
