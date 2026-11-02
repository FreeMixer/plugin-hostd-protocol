/*
 * What an installed libplugin-hostd-protocol does with the bytes a client sends (mod-audio/mod-host#113).
 *
 * The library is started with a 1024-byte receive buffer and a receive callback that answers each command with
 * its length, so the client sees how many commands the library ran and where it split them. Without #113 a
 * command of exactly 1024 bytes waits for more data that never comes, and of two commands in one write only the
 * first runs. Built against the installed headers and library: tests/install-check.sh.
 */

#include <socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int g_port;
static int g_failures;

static void answer_with_length(msg_t *msg)
{
    char answer[32];
    snprintf(answer, sizeof(answer), "%zu", strlen(msg->data));
    socket_send(msg->sender_id, answer, strlen(answer) + 1);
}

static void *server_thread(void *arg)
{
    (void)arg;
    for (;;)
        socket_run(0);
    return NULL;
}

/* a port nothing listens on now; socket_start() binds it with SO_REUSEPORT */
static int free_port(void)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd < 0 || bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
    {
        perror("free port");
        exit(2);
    }
    close(fd);
    return ntohs(addr.sin_port);
}

/* sends each chunk as its own write, then collects the answers for up to two seconds */
static void check(const char *name, const char *expected, int nchunks, const char **chunks, const size_t *sizes)
{
    struct sockaddr_in addr;
    struct pollfd pfd;
    char got[256], buf[256];
    size_t len = 0;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(g_port);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        perror("connect");
        exit(2);
    }

    for (int i = 0; i < nchunks; i++)
    {
        if (i > 0)
            usleep(100 * 1000);
        if (send(fd, chunks[i], sizes[i], 0) != (ssize_t)sizes[i])
        {
            perror("send");
            exit(2);
        }
    }

    got[0] = '\0';
    pfd.fd = fd;
    pfd.events = POLLIN;
    while (poll(&pfd, 1, 2000) > 0)
    {
        ssize_t count = recv(fd, buf, sizeof(buf), 0);
        if (count <= 0)
            break;
        for (ssize_t i = 0; i < count && len + 2 < sizeof(got); i++)
            got[len++] = buf[i] ? buf[i] : ' ';
        got[len] = '\0';
    }
    if (len > 0 && got[len - 1] == ' ')
        got[len - 1] = '\0';
    close(fd);

    if (strcmp(got, expected) == 0)
    {
        printf("ok      %s\n", name);
    }
    else
    {
        printf("FAILED  %s: expected answers \"%s\", got \"%s\"\n", name, expected, got);
        g_failures++;
    }

    /* let socket_run() see the disconnection and wait for the next client */
    usleep(100 * 1000);
}

int main(void)
{
    pthread_t thread;
    char *exact = malloc(1024);

    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);

    g_port = free_port();
    if (socket_start(g_port, 0, 1024) != 0)
        return 2;
    socket_set_receive_cb(answer_with_length);
    pthread_create(&thread, NULL, server_thread, NULL);

    {
        const char *c[] = { "cpu_load" };
        const size_t n[] = { 9 };
        check("one command", "8", 1, c, n);
    }

    {
        /* "cpu_load" padded with spaces to exactly the receive buffer, its NUL included */
        memset(exact, ' ', 1023);
        memcpy(exact, "cpu_load", 8);
        exact[1023] = '\0';
        const char *c[] = { exact };
        const size_t n[] = { 1024 };
        check("command of exactly 1024 bytes", "1023", 1, c, n);
    }

    {
        const char *c[] = { "cpu_load\0bypass 0 1" };
        const size_t n[] = { 20 };
        check("two commands in one write", "8 10", 1, c, n);
    }

    {
        const char *c[] = { "cpu_", "load" };
        const size_t n[] = { 4, 5 };
        check("one command in two writes", "8", 2, c, n);
    }

    free(exact);

    if (g_failures)
    {
        printf("%d check(s) failed: this library does not carry mod-audio/mod-host#113\n", g_failures);
        return 1;
    }

    printf("all checks passed\n");
    return 0;
}
