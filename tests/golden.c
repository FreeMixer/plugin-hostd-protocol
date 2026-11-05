/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * plugin-hostd-protocol is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * plugin-hostd-protocol is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along with plugin-hostd-protocol.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

/* The golden transcript: every command of a scenario file and every seed file given, sent over a real TCP socket to
 * the library's own server with a recording backend behind it, once for each of the nine answers the backend can
 * give. For each exchange it writes the command, the reply and the backend calls it made, bytes outside printable
 * ASCII escaped, so two libraries can be compared byte for byte:
 *
 *   golden <scenario file> [<seed file> | dispatch:<seed file> ...] > transcript
 *
 * A seed named dispatch:<file> carries fuzz_dispatch's salt byte first; it is dropped, the backend's answer being
 * walked through all nine anyway. The program uses only the public headers, so the same source built against
 * mod-host-protocol gives mod-host's transcript (make golden-modhost). */

#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <host-backend.h>
#include <host-dispatch.h>
#include <host-errors.h>
#include <host-scenario.h>
#include <protocol.h>
#include <socket.h>

#define GOLDEN_PORT_DEFAULT 15570
#define GOLDEN_BUFFER_SIZE  65536
#define GOLDEN_MAX_INPUTS   1024
#define GOLDEN_SALTS        9

typedef struct {
    char *name;
    char *command;
} input_t;

static input_t g_inputs[GOLDEN_MAX_INPUTS];
static int g_input_count;
static int g_port;
static int g_failed;
static unsigned g_salt;
static char g_calls[4096];

static void record(const char *fmt, ...)
{
    size_t used = strlen(g_calls);
    va_list ap;

    if (used + 2 >= sizeof(g_calls))
        return;
    if (used)
        g_calls[used++] = ' ';
    va_start(ap, fmt);
    vsnprintf(g_calls + used, sizeof(g_calls) - used, fmt, ap);
    va_end(ap);
}

static int answer(int key)
{
    static const int codes[GOLDEN_SALTS] = {
        SUCCESS, ERR_INSTANCE_INVALID, ERR_INSTANCE_ALREADY_EXISTS, ERR_INSTANCE_NON_EXISTS,
        ERR_INSTANCE_UNLICENSED, ERR_HOST_INVALID_URI, ERR_INVALID_OPERATION, ERR_JACK_PORT_CONNECTION,
        ERR_HOST_CANT_LOAD_STATE,
    };
    return codes[((unsigned)key + g_salt) % GOLDEN_SALTS];
}

static int b_add(const char *uri, int instance, const char *client)
{
    record("add(%s,%d,%s)", uri, instance, client ? client : "NULL");
    return answer(instance) == SUCCESS ? instance : answer(instance);
}
static int b_remove(int instance) { record("remove(%d)", instance); return answer(instance); }
static int b_bypass(int instance, int value) { record("bypass(%d,%d)", instance, value); return answer(instance); }
static int b_param_set(int instance, const char *symbol, float value)
{
    record("param_set(%d,%s,%a)", instance, symbol, (double)value);
    return answer(instance);
}
static int b_param_get(int instance, const char *symbol, float *value)
{
    record("param_get(%d,%s)", instance, symbol);
    *value = (float)instance / 4.0f;
    return answer(instance);
}
static int b_preset_load(int instance, const char *uri) { record("preset_load(%d,%s)", instance, uri); return answer(instance); }
static int b_state_save(const char *dir) { record("state_save(%s)", dir); return answer((int)strlen(dir)); }
static int b_state_load(const char *dir) { record("state_load(%s)", dir); return answer((int)strlen(dir)); }
static int b_connect(const char *a, const char *b)
{
    record("connect(%s,%s)", a, b);
    return answer((int)(strlen(a) + strlen(b)));
}
static int b_disconnect(const char *a, const char *b)
{
    record("disconnect(%s,%s)", a, b);
    return answer((int)(strlen(a) + strlen(b)));
}
static int b_monitor_output(int instance, const char *symbol)
{
    record("monitor_output(%d,%s)", instance, symbol);
    return answer(instance);
}
static int b_track_info(int instance, const char *name, const char *color, const char *kind)
{
    record("track_info(%d,%s,%s,%s)", instance, name, color, kind ? kind : "NULL");
    return answer(instance);
}
static int b_remote_pages(int instance)
{
    record("remote_pages(%d)", instance);
    return answer(instance) == SUCCESS ? instance % 4 : answer(instance);
}
static int b_remote_page_get(int instance, int page, host_remote_page_t *out)
{
    int i;

    record("remote_page_get(%d,%d)", instance, page);
    if (answer(instance) != SUCCESS)
        return answer(instance);
    memset(out, 0, sizeof(*out));
    out->id = (uint32_t)page;
    strcpy(out->section, "Main \"A\"\\");
    strcpy(out->name, "Page");
    for (i = 0; i < HOST_REMOTE_PAGE_SLOTS; i++)
        strcpy(out->slots[i], i % 2 ? "-" : "gain");
    return SUCCESS;
}
/* a port's range or default need not be finite: a range times the sample rate can overflow a float */
static int b_param_info(int instance, const char *symbol, host_param_info_t *info)
{
    record("param_info(%d,%s)", instance, symbol);
    if (answer(instance) != SUCCESS)
        return answer(instance);
    info->unit = "hz";
    info->scale = "log";
    info->min = 20;
    info->max = 20000;
    info->def = 440.125;
    info->step = (double)(instance % 3) / 8.0;
    if (strcmp(symbol, "inf") == 0)
        info->max = INFINITY;
    else if (strcmp(symbol, "-inf") == 0)
        info->min = -INFINITY;
    else if (strcmp(symbol, "nan") == 0)
        info->def = NAN;
    else if (strcmp(symbol, "tiny") == 0)
        info->min = -0.0, info->max = 1e-7, info->def = 1e21;
    info->stable_symbol = "stable";
    return SUCCESS;
}

static const host_backend_t g_backend = {
    b_add, b_remove, b_bypass, b_param_set, b_param_get, b_preset_load, b_state_save, b_state_load,
    b_connect, b_disconnect,
};
static const host_plugin_info_t g_info = { b_track_info, b_remote_pages, b_remote_page_get, b_param_info };

static void escaped(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
    {
        if (*p == '\\')
            fputs("\\\\", stdout);
        else if (*p < 0x20 || *p > 0x7e)
            printf("\\x%02x", *p);
        else
            putchar(*p);
    }
    putchar('\n');
}

static char *read_file(const char *path, size_t skip)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    size_t n;

    if (!f)
        return NULL;
    buf = calloc(1, GOLDEN_BUFFER_SIZE + 1);
    n = fread(buf, 1, GOLDEN_BUFFER_SIZE, f);
    fclose(f);
    if (n < skip)
        n = skip;
    memmove(buf, buf + skip, n - skip);
    buf[n - skip] = '\0';
    return buf;
}

static int read_reply(int fd, char *buf, size_t size)
{
    size_t got = 0;

    while (got < size - 1)
    {
        ssize_t n = recv(fd, buf + got, 1, 0);
        if (n <= 0)
            return -1;
        if (buf[got] == '\0')
            return 0;
        got++;
    }
    buf[got] = '\0';
    return -1;
}

static void *client_thread(void *arg)
{
    static char reply[GOLDEN_BUFFER_SIZE];
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int i;

    (void)arg;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        g_failed = 1;
        close(fd);
        return NULL;
    }

    for (g_salt = 0; g_salt < GOLDEN_SALTS; g_salt++)
    {
        for (i = 0; i < g_input_count; i++)
        {
            g_calls[0] = '\0';
            if (send(fd, g_inputs[i].command, strlen(g_inputs[i].command) + 1, MSG_NOSIGNAL) < 0 ||
                read_reply(fd, reply, sizeof(reply)) != 0)
            {
                fprintf(stderr, "no reply to %s (answer %u)\n", g_inputs[i].name, g_salt);
                g_failed = 1;
                close(fd);
                return NULL;
            }
            printf("# %s answer %u\n> ", g_inputs[i].name, g_salt);
            escaped(g_inputs[i].command);
            fputs("< ", stdout);
            escaped(reply);
            fputs("= ", stdout);
            escaped(g_calls);
        }
    }
    close(fd);
    return NULL;
}

static const host_scenario_var_t g_vars[] = {
    { "URI", "urn:golden:amp" },
    { "BAD_URI", "urn:golden:missing" },
    { "PARAM", "gain" },
    { "DIR", "/tmp/golden" },
    { "PORT_A", "a:out" },
    { "PORT_B", "b:in" },
    { "REMOVE_TWICE", "resp -3" },
};

int main(int argc, char **argv)
{
    static host_scenario_list_t list;
    const char *env = getenv("GOLDEN_PORT");
    pthread_t thread;
    int i;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <scenario file> [<seed file> | dispatch:<seed file> ...]\n", argv[0]);
        return 2;
    }
    if (host_scenario_load(argv[1], g_vars, (int)(sizeof(g_vars) / sizeof(g_vars[0])), &list) <= 0)
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    for (i = 0; i < list.count && g_input_count < GOLDEN_MAX_INPUTS; i++)
    {
        char name[64];
        snprintf(name, sizeof(name), "scenario-%d", i + 1);
        g_inputs[g_input_count].name = strdup(name);
        g_inputs[g_input_count].command = strdup(list.lines[i].command);
        g_input_count++;
    }
    host_scenario_free(&list);
    for (i = 2; i < argc && g_input_count < GOLDEN_MAX_INPUTS; i++)
    {
        int dispatch = strncmp(argv[i], "dispatch:", 9) == 0;
        const char *path = dispatch ? argv[i] + 9 : argv[i];
        char *command = read_file(path, dispatch ? 1 : 0);
        if (!command)
        {
            fprintf(stderr, "cannot read %s\n", path);
            return 2;
        }
        g_inputs[g_input_count].name = strdup(argv[i]);
        g_inputs[g_input_count].command = command;
        g_input_count++;
    }

    g_port = env ? atoi(env) : GOLDEN_PORT_DEFAULT;
    if (socket_start(g_port, 0, GOLDEN_BUFFER_SIZE) < 0)
    {
        fprintf(stderr, "socket_start on port %d failed\n", g_port);
        return 2;
    }
    socket_set_receive_cb(protocol_parse);
    host_dispatch_register(&g_backend);
    host_dispatch_register_monitor_output(b_monitor_output);
    host_dispatch_register_plugin_info(&g_info);
    host_dispatch_register_unsupported();
#ifdef PROTOCOL_HELLO
    /* this library's hello: a line that does not follow a hello is answered as before (deviations.txt) */
    protocol_add_hello();
#endif

    pthread_create(&thread, NULL, client_thread, NULL);
    socket_run(0);
    pthread_join(thread, NULL);
    socket_finish();
    protocol_remove_commands();

    for (i = 0; i < g_input_count; i++)
    {
        free(g_inputs[i].name);
        free(g_inputs[i].command);
    }
    return g_failed;
}
