/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * hello and request tags (doc/protocol.md, Sessions), through the library's own server on a real TCP socket and its
 * feedback port, as a host meets them:
 *   - a host that registers hello answers a client that never sends it exactly as a host that does not: the same lines,
 *     the same answers, a line that starts with "@" included;
 *   - hello grants the extensions it knows and asks for, once each, and no other; a wrong version is -902 and changes
 *     nothing;
 *   - a tagged request's answer, a refusal or a parse error, carries its tag; a word "@" alone, or one with a character
 *     no tag has, is no tag;
 *   - a hello that drops tags is answered with the tag it was sent with, and the next tagged line is no tag;
 *   - an event a tagged request causes goes out on the feedback port untagged;
 *   - a session granted errors reads a refusal's error word after its number, for a number the protocol declares a
 *     code for and for nothing else;
 *   - a new connection starts with nothing granted.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <host-backend.h>
#include <host-dispatch.h>
#include <host-errors.h>
#include <protocol.h>
#include <socket.h>

#define HELLO_PORT_DEFAULT 15590

static int g_port;
static int g_failures;
static volatile int g_running = 1;

static int b_param_get(int instance, const char *symbol, float *value)
{
    (void)symbol;
    *value = 0.25f;
    return instance == 3 ? SUCCESS : ERR_INSTANCE_NON_EXISTS;
}

static int b_remove(int instance)
{
    return instance == 3 ? SUCCESS : ERR_INSTANCE_NON_EXISTS;
}

static const host_backend_t g_backend = { .remove = b_remove, .param_get = b_param_get };

/* emit <instance>: an event on the feedback port, then resp 0 */
static void emit_cb(proto_t *proto)
{
    host_dispatch_remote_pages_changed(atoi(proto->list[1]));
    protocol_response_int(SUCCESS, proto);
}

/* refuse <n> [<word>]: resp <n>, and the word after it when one is given */
static void refuse_cb(proto_t *proto)
{
    char reply[128];

    snprintf(reply, sizeof(reply), "resp %d%s%s", atoi(proto->list[1]), proto->list_count > 2 ? " " : "",
             proto->list_count > 2 ? proto->list[2] : "");
    protocol_response(reply, proto);
}

static void *server_thread(void *arg)
{
    (void)arg;
    while (g_running)
        socket_run(0);
    return NULL;
}

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

static void expect(conn_t *c, const char *what, const char *command, const char *want)
{
    char got[256];

    exchange(c, command, got, sizeof(got));
    if (strcmp(got, want) != 0)
    {
        printf("FAIL %s '%s': got '%s', want '%s'\n", what, command, got, want);
        g_failures++;
        return;
    }
    printf("ok   %s '%s' -> '%s'\n", what, command, got);
}

static void expect_feedback(conn_t *c, const char *what, const char *want)
{
    char got[256];

    read_line(c->fb, got, sizeof(got));
    if (strcmp(got, want) != 0)
    {
        printf("FAIL %s: the feedback port got '%s', want '%s'\n", what, got, want);
        g_failures++;
        return;
    }
    printf("ok   %s: the feedback port got '%s'\n", what, got);
}

/* what a client that never sends hello sends: answered the same with and without hello registered */
static const char *const g_plain[] = {
    "param_get 3 gain", "param_get 9 gain", "remove", "remove 3 4", "frobnicate 1", "@t1 param_get 3 gain", "@t1",
    "@ param_get 3 gain", "@a@b remove 3", "emit 3", "help", "\"param_get\" 3 gain", "param_get  3   gain", "",
    "refuse -3", "refuse -902",
};
#define PLAIN_COUNT (sizeof(g_plain) / sizeof(g_plain[0]))

static void answers_of(char out[][256])
{
    conn_t c = open_session();
    char fb[256];
    size_t i;

    for (i = 0; i < PLAIN_COUNT; i++)
    {
        /* an empty line is not answered, as mod-host does not answer it */
        if (g_plain[i][0] == '\0')
        {
            send(c.cmd, "", 1, MSG_NOSIGNAL);
            continue;
        }
        exchange(&c, g_plain[i], out[i], 256);
        if (strcmp(g_plain[i], "emit 3") == 0)
            read_line(c.fb, fb, sizeof(fb));
    }
    close_session(&c);
}

static void register_host(int hello)
{
    protocol_remove_commands();
    host_dispatch_register(&g_backend);
    host_dispatch_register_unsupported();
    protocol_add_command("emit %i", emit_cb);
    protocol_add_command("refuse %i ...", refuse_cb);
    if (hello)
        protocol_add_hello();
}

int main(void)
{
    static char without[PLAIN_COUNT][256], with[PLAIN_COUNT][256];
    const char *env = getenv("HELLO_PORT");
    pthread_t thread;
    conn_t c;
    size_t i;

    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);
    g_port = env ? atoi(env) : HELLO_PORT_DEFAULT;
    if (socket_start(g_port, g_port + 1, 4096) != 0)
    {
        printf("FAIL socket_start on ports %d and %d\n", g_port, g_port + 1);
        return 1;
    }
    socket_set_receive_cb(protocol_parse);
    pthread_create(&thread, NULL, server_thread, NULL);

    /* -- a client that never sends hello */
    register_host(0);
    answers_of(without);
    register_host(1);
    answers_of(with);
    for (i = 0; i < PLAIN_COUNT; i++)
    {
        if (g_plain[i][0] == '\0')
            continue;
        if (strcmp(without[i], with[i]) != 0)
        {
            printf("FAIL no hello '%s': '%s' without hello registered, '%s' with it\n", g_plain[i], without[i],
                   with[i]);
            g_failures++;
        }
        else
            printf("ok   no hello '%s' -> '%s' with and without hello registered\n", g_plain[i], with[i]);
    }

    /* -- hello grants what it knows */
    c = open_session();
    expect(&c, "hello", "hello 1", "resp 0 1");
    expect(&c, "no grant", "@t1 param_get 3 gain", "not found");
    expect(&c, "hello", "hello 1 tags no_such_extension tags", "resp 0 1 tags");
    expect(&c, "a later version", "hello 7 tags", "resp 0 1 tags");
    expect(&c, "a wrong version", "hello 0", "resp -902");
    expect(&c, "a wrong version", "hello x tags", "resp -902");
    expect(&c, "a wrong version", "hello", "few arguments");
    /* a refused hello changes nothing: tags hold */
    expect(&c, "tagged", "@t1 param_get 3 gain", "@t1 resp 0 0.2500");
    expect(&c, "tagged refusal", "@a.b-c_D9 param_get 9 gain", "@a.b-c_D9 resp -3");
    expect(&c, "tagged parse error", "@t2 remove", "@t2 few arguments");
    expect(&c, "tagged parse error", "@t3 remove 3 4", "@t3 many arguments");
    expect(&c, "tagged unknown verb", "@t4 frobnicate", "@t4 not found");
    expect(&c, "tagged, nothing after it", "@t5", "@t5 not found");
    expect(&c, "untagged", "param_get 3 gain", "resp 0 0.2500");
    expect(&c, "no tag: @ alone", "@ param_get 3 gain", "not found");
    expect(&c, "no tag: a character no tag has", "@a@b remove 3", "not found");
    expect(&c, "no tag: a quote", "@\"a\" remove 3", "not found");
    /* an event a tagged request causes is not tagged */
    expect(&c, "tagged, with an event", "@ev emit 3", "@ev resp 0");
    expect_feedback(&c, "the event of a tagged request", "remote_pages_changed 3");
    /* the hello that drops tags is answered with its tag; the next line has none */
    expect(&c, "tagged hello", "@t6 hello 1", "@t6 resp 0 1");
    expect(&c, "tags dropped", "@t7 param_get 3 gain", "not found");
    expect(&c, "hello", "hello 1 tags", "resp 0 1 tags");
    expect(&c, "no error word", "refuse -3", "resp -3");

    /* -- error words */
    expect(&c, "hello", "hello 1 errors", "resp 0 1 errors");
    expect(&c, "an error word", "param_get 9 gain", "resp -3 reply.host.instance-non-exists");
    expect(&c, "an error word", "refuse -902", "resp -902 reply.host.invalid-operation");
    expect(&c, "no word declared", "refuse -2", "resp -2");
    expect(&c, "a refusal with values", "refuse -3 extra", "resp -3 extra");
    expect(&c, "a success", "param_get 3 gain", "resp 0 0.2500");
    expect(&c, "a parse refusal", "remove", "few arguments");
    expect(&c, "a refused hello", "hello 0", "resp -902 reply.host.invalid-operation");
    expect(&c, "hello", "hello 1 errors tags", "resp 0 1 tags errors");
    expect(&c, "tagged, with an error word", "@e1 param_get 9 gain", "@e1 resp -3 reply.host.instance-non-exists");
    expect(&c, "hello", "hello 1 tags", "resp 0 1 tags");
    expect(&c, "words dropped", "param_get 9 gain", "resp -3");
    close_session(&c);

    /* -- a new connection starts with nothing granted */
    c = open_session();
    expect(&c, "a new connection", "@t1 param_get 3 gain", "not found");
    expect(&c, "a new connection", "param_get 3 gain", "resp 0 0.2500");
    close_session(&c);

    g_running = 0;
    socket_finish();
    pthread_join(thread, NULL);
    protocol_remove_commands();

    printf("%s\n", g_failures == 0 ? "hello ok" : "hello FAILED");
    return g_failures == 0 ? 0 : 1;
}
