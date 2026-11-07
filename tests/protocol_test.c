/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of mod-host.
 *
 * mod-host is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mod-host is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mod-host.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* Drives the socket protocol end to end against a fake backend, no jack needed:
 * every reply is compared byte for byte and every backend call is recorded.
 * Then runs tests/host-scenarios.txt through the direct exchange against a
 * reference backend, the same way a host in one process is tested. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../include/socket.h"
#include "../include/protocol.h"
#include "../include/host-dispatch.h"
#include "../include/host-errors.h"
#include "../include/host-scenario.h"
#include "../include/plugin-hostd/protocol.h"
#include "../include/utils.h"

#define TEST_PORT_DEFAULT   15556
#define TEST_BUFFER_SIZE    1024

typedef struct EXCHANGE_T {
    const char *command;
    const char *reply;
} exchange_t;

typedef struct SESSION_T {
    int port;
    const exchange_t *exchanges;
} session_t;

static char g_calls[2048];
static int g_failures;

static void record(const char *call)
{
    strncat(g_calls, call, sizeof(g_calls) - strlen(g_calls) - 1);
}

static int fake_add(const char *uri, int instance, const char *client_name)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "add(%s,%i,%s) ", uri, instance, client_name ? client_name : "-");
    record(buf);
    return instance;
}

static int fake_remove(int instance)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "remove(%i) ", instance);
    record(buf);
    return SUCCESS;
}

static int fake_bypass(int instance, int value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "bypass(%i,%i) ", instance, value);
    record(buf);
    return SUCCESS;
}

static int fake_param_set(int instance, const char *symbol, float value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "param_set(%i,%s,%f) ", instance, symbol, value);
    record(buf);
    return SUCCESS;
}

static int fake_param_get(int instance, const char *symbol, float *value)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "param_get(%i,%s) ", instance, symbol);
    record(buf);
    *value = 0.5f;
    return SUCCESS;
}

static int fake_preset_load(int instance, const char *uri)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "preset_load(%i,%s) ", instance, uri);
    record(buf);
    return ERR_LV2_INVALID_PRESET_URI;
}

static int fake_state_save(const char *dir)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "state_save(%s) ", dir);
    record(buf);
    return SUCCESS;
}

static int fake_state_load(const char *dir)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "state_load(%s) ", dir);
    record(buf);
    return ERR_LV2_CANT_LOAD_STATE;
}

static int fake_connect(const char *port_a, const char *port_b)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "connect(%s,%s) ", port_a, port_b);
    record(buf);
    return SUCCESS;
}

static int fake_disconnect(const char *port_a, const char *port_b)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "disconnect(%s,%s) ", port_a, port_b);
    record(buf);
    return ERR_JACK_PORT_DISCONNECTION;
}

static int fake_monitor_output(int instance, const char *symbol)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "monitor_output(%i,%s) ", instance, symbol);
    record(buf);
    if (instance != 3)
        return ERR_INSTANCE_NON_EXISTS;
    host_dispatch_output_set(instance, symbol, 0.25f);
    host_dispatch_remote_pages_changed(instance);
    return SUCCESS;
}

static int fake_track_info(int instance, const char *name, const char *color, const char *kind)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "track_info(%i,%s,%s,%s) ", instance, name, color, kind ? kind : "-");
    record(buf);
    return instance == 3 ? SUCCESS : ERR_INSTANCE_NON_EXISTS;
}

static int fake_remote_pages(int instance)
{
    return instance == 3 ? 2 : ERR_INSTANCE_NON_EXISTS;
}

static int fake_remote_page_get(int instance, int page, host_remote_page_t *page_out)
{
    if (instance != 3)
        return ERR_INSTANCE_NON_EXISTS;
    if (page != 0)
        return ERR_INVALID_OPERATION;
    page_out->id = 7;
    strcpy(page_out->section, "Main \"A\"");
    strcpy(page_out->name, "Page 1");
    strcpy(page_out->slots[0], "gain");
    strcpy(page_out->slots[1], "level");
    return SUCCESS;
}

static int fake_param_info(int instance, const char *symbol, host_param_info_t *info)
{
    if (instance != 3)
        return ERR_INSTANCE_NON_EXISTS;
    if (strcmp(symbol, "time") == 0)
    {
        info->unit = "ms"; info->scale = "linear";
        info->min = 1; info->max = 2000; info->def = 350; info->step = 1;
        info->stable_symbol = "time_ms";
        return SUCCESS;
    }
    if (strcmp(symbol, "cutoff") == 0)
    {
        info->unit = "hz"; info->scale = "log";
        info->min = 20; info->max = 20000; info->def = 8000; info->step = 0;
        info->stable_symbol = "cutoff";
        return SUCCESS;
    }
    if (strcmp(symbol, "mix") == 0)
    {
        info->unit = "none"; info->scale = "linear";
        info->min = -0.0; info->max = 0.1; info->def = 1.0 / 3.0; info->step = 0;
        info->stable_symbol = "mix";
        return SUCCESS;
    }
    if (strcmp(symbol, "range") == 0)
    {
        info->unit = "none"; info->scale = "linear";
        info->min = -1e-7; info->max = 1e21; info->def = 0.000001; info->step = 1e20;
        info->stable_symbol = "range";
        return SUCCESS;
    }
    return ERR_HOST_INVALID_PARAM_SYMBOL;
}

static const host_plugin_info_t g_fake_info = {
    fake_track_info,
    fake_remote_pages,
    fake_remote_page_get,
    fake_param_info,
};

static const host_plugin_info_t g_empty_info = { NULL, NULL, NULL, NULL };

static const host_backend_t g_fake_backend = {
    fake_add,
    fake_remove,
    fake_bypass,
    fake_param_set,
    fake_param_get,
    fake_preset_load,
    fake_state_save,
    fake_state_load,
    fake_connect,
    fake_disconnect,
};

#define NAME_16  "aaaaaaaaaaaaaaaa"
#define NAME_255 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 \
                 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 NAME_16 "aaaaaaaaaaaaaaa"
#define NAME_256 NAME_255 "a"

static const exchange_t g_fake_exchanges[] = {
    { "add http://x/y 3 chan1",     "resp 3" },
    { "add http://x/y 4",           "resp 4" },
    { "add clap:/usr/lib/clap/Surge XT.clap#org.surge 5 chan2", "resp 5" },
    { "add \"clap:/a b/c  d.clap#x\" 6", "resp 6" },
    { "bypass 3 1",                 "resp 0" },
    { "param_set 3 gain 0.5",       "resp 0" },
    { "param_get 3 gain",           "resp 0 0.5000" },
    { "preset_load 3 urn:p",        "resp -104" },
    { "connect a:out b:in",         "resp 0" },
    { "disconnect a:out b:in",      "resp -206" },
    { "state_save /tmp/s",          "resp 0" },
    { "state_load /tmp/s",          "resp -105" },
    { "monitor_output 3 level",     "resp 1" },
    { "monitor_output 9 level",     "resp 0" },
    { "track_info 3 \"Kick \\\"In\\\" \xc3\xb1\" #FF8000 bus", "resp 0" },
    { "track_info 3 \"\" -",          "resp 0" },
    { "track_info 3 Kick #ff8000",  "resp 0" },
    { "track_info 9 Kick -",        "resp -3" },
    { "track_info 3 Kick #FF800",   "resp -902" },
    { "track_info 3 Kick FF8000",   "resp -902" },
    { "track_info 3 Kick - aux",    "resp -902" },
    { "track_info 3 Kick - bus x",  "resp -902" },
    { "track_info 3 \"a\tb\" -",     "resp -902" },
    { "track_info 3 \xff -",        "resp -902" },
    { "track_info 3 " NAME_256 " -", "resp -902" },
    { "track_info 3 " NAME_255 " -", "resp 0" },
    { "remote_pages 3",             "resp 2" },
    { "remote_pages 9",             "resp -3" },
    { "remote_page_get 3 0",        "resp 0 7 \"Main \\\"A\\\"\" \"Page 1\" gain level - - - - - -" },
    { "remote_page_get 3 2",        "resp -902" },
    { "param_info 3 time",          "resp 0 ms linear 1 2000 350 1 time_ms" },
    { "param_info 3 cutoff",        "resp 0 hz log 20 20000 8000 0 cutoff" },
    { "param_info 3 mix",           "resp 0 none linear 0 0.1 0.3333333333333333 0 mix" },
    { "param_info 3 range",         "resp 0 none linear -1e-7 1e+21 0.000001 100000000000000000000 range" },
    { "param_info 3 nope",          "resp -103" },
    { "param_info 9 time",          "resp -3" },
    { "remove 3",                   "resp 0" },
    { "licensee 3",                 "resp -902" },
    { "cpu_load",                   "not found" },
    { "param_get 3",                "few arguments" },
    { NULL, NULL }
};

static const char g_fake_calls[] =
    "add(http://x/y,3,chan1) add(http://x/y,4,-) add(clap:/usr/lib/clap/Surge XT.clap#org.surge,5,chan2) "
    "add(clap:/a b/c  d.clap#x,6,-) bypass(3,1) param_set(3,gain,0.500000) "
    "param_get(3,gain) preset_load(3,urn:p) connect(a:out,b:in) disconnect(a:out,b:in) "
    "state_save(/tmp/s) state_load(/tmp/s) monitor_output(3,level) monitor_output(9,level) "
    "track_info(3,Kick \"In\" \xc3\xb1,#FF8000,bus) track_info(3,,-,-) track_info(3,Kick,#ff8000,-) "
    "track_info(9,Kick,-,-) track_info(3," NAME_255 ",-,-) remove(3) ";

/* a backend with nothing behind it answers every verb with ERR_INVALID_OPERATION */
static const host_backend_t g_empty_backend = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

static const exchange_t g_empty_exchanges[] = {
    { "add http://x/y 3",           "resp -902" },
    { "param_get 3 gain",           "resp -902" },
    { "state_save /tmp/s",          "resp -902" },
    { "monitor_output 3 level",     "resp -902" },
    { "track_info 3 Kick -",        "resp -902" },
    { "remote_pages 3",             "resp -902" },
    { "remote_page_get 3 0",        "resp -902" },
    { "param_info 3 time",          "resp -902" },
    { NULL, NULL }
};

/* A reference backend: one plugin, one instance table, one control, bypass as the host's. */
#define REF_URI         "urn:ref:plugin"
#define REF_INSTANCES   16

static struct {
    int present;
    int bypass;
    float gain;
} g_ref[REF_INSTANCES];

static int ref_add(const char *uri, int instance, const char *client_name)
{
    (void)client_name;
    if (instance < 0 || instance >= REF_INSTANCES)
        return ERR_INSTANCE_INVALID;
    if (g_ref[instance].present)
        return ERR_INSTANCE_ALREADY_EXISTS;
    if (strcmp(uri, REF_URI) != 0)
        return ERR_HOST_INVALID_URI;
    g_ref[instance].present = 1;
    g_ref[instance].bypass = 0;
    g_ref[instance].gain = 1.0f;
    return instance;
}

static int ref_remove(int instance)
{
    if (instance < 0 || instance >= REF_INSTANCES || !g_ref[instance].present)
        return ERR_INSTANCE_NON_EXISTS;
    g_ref[instance].present = 0;
    return SUCCESS;
}

static int ref_bypass(int instance, int value)
{
    if (instance < 0 || instance >= REF_INSTANCES || !g_ref[instance].present)
        return ERR_INSTANCE_NON_EXISTS;
    g_ref[instance].bypass = value != 0;
    return SUCCESS;
}

static int ref_param_set(int instance, const char *symbol, float value)
{
    if (instance < 0 || instance >= REF_INSTANCES || !g_ref[instance].present)
        return ERR_INSTANCE_NON_EXISTS;
    if (strcmp(symbol, ":bypass") == 0)
        return ref_bypass(instance, value > 0.5f);
    if (strcmp(symbol, "gain") != 0)
        return ERR_HOST_INVALID_PARAM_SYMBOL;
    g_ref[instance].gain = value;
    return SUCCESS;
}

static int ref_param_get(int instance, const char *symbol, float *value)
{
    if (instance < 0 || instance >= REF_INSTANCES || !g_ref[instance].present)
        return ERR_INSTANCE_NON_EXISTS;
    if (strcmp(symbol, ":bypass") == 0)
    {
        *value = g_ref[instance].bypass ? 1.0f : 0.0f;
        return SUCCESS;
    }
    if (strcmp(symbol, "gain") != 0)
        return ERR_HOST_INVALID_PARAM_SYMBOL;
    *value = g_ref[instance].gain;
    return SUCCESS;
}

static int ref_preset_load(int instance, const char *uri)
{
    (void)uri;
    if (instance < 0 || instance >= REF_INSTANCES || !g_ref[instance].present)
        return ERR_INSTANCE_NON_EXISTS;
    return ERR_HOST_INVALID_PRESET_URI;
}

static float g_ref_saved[REF_INSTANCES];

static int ref_state_save(const char *dir)
{
    int i;
    (void)dir;
    for (i = 0; i < REF_INSTANCES; i++)
        g_ref_saved[i] = g_ref[i].gain;
    return SUCCESS;
}

static int ref_state_load(const char *dir)
{
    int i;
    (void)dir;
    for (i = 0; i < REF_INSTANCES; i++)
        if (g_ref[i].present)
            g_ref[i].gain = g_ref_saved[i];
    return SUCCESS;
}

static const host_backend_t g_ref_backend = {
    .add = ref_add,
    .remove = ref_remove,
    .bypass = ref_bypass,
    .param_set = ref_param_set,
    .param_get = ref_param_get,
    .preset_load = ref_preset_load,
    .state_save = ref_state_save,
    .state_load = ref_state_load,
    .connect = fake_connect,
    .disconnect = fake_disconnect,
};

/* the same with no presets, no state and no wiring: the runner must read -902 and skip */
static const host_backend_t g_ref_bare_backend = {
    .add = ref_add,
    .remove = ref_remove,
    .bypass = ref_bypass,
    .param_set = ref_param_set,
    .param_get = ref_param_get,
};

static const host_scenario_var_t g_ref_vars[] = {
    { "URI", REF_URI },
    { "BAD_URI", "urn:ref:missing" },
    { "PARAM", "gain" },
    { "DIR", "/tmp" },
    { "PORT_A", "a:out" },
    { "PORT_B", "b:in" },
    { "REMOVE_TWICE", "resp -3" },
};

static int run_scenarios(const char *path, const host_backend_t *backend)
{
    host_scenario_list_t list;
    host_scenario_direct_t direct;
    int failures;

    memset(g_ref, 0, sizeof(g_ref));
    if (host_scenario_load(path, g_ref_vars, 7, &list) <= 0)
    {
        printf("FAIL cannot load %s\n", path);
        return 1;
    }
    if (host_scenario_direct_open(&direct, backend) != 0)
    {
        printf("FAIL direct exchange\n");
        host_scenario_free(&list);
        return 1;
    }
    failures = host_scenario_run(&list, host_scenario_direct_exchange, &direct, backend);
    host_scenario_direct_close(&direct);
    host_scenario_free(&list);
    if (failures != 0)
    {
        printf("FAIL scenarios: %d\n", failures);
        return 1;
    }
    return 0;
}

/* the plugin's own window: a host with only the backend table behind it has no window, and refuses both verbs with
 * -902, the answer an optional verb owes when it is not served; never "not found", never misread */
static int run_window_verbs(void)
{
    static const char *const lines[] = { "show_external_ui 3", "hide_external_ui 3" };
    host_scenario_direct_t direct;
    char reply[64];
    size_t n;
    int failures = 0;

    memset(g_ref, 0, sizeof(g_ref));
    if (host_scenario_direct_open(&direct, &g_ref_backend) != 0)
    {
        printf("FAIL direct exchange for the window verbs\n");
        return 1;
    }
    for (n = 0; n < sizeof(lines) / sizeof(lines[0]); n++)
        if (host_scenario_direct_exchange(&direct, lines[n], reply, sizeof(reply)) < 0 || strcmp(reply, "resp -902"))
            failures++, printf("FAIL '%s': got '%s', want 'resp -902'\n", lines[n], reply);
    host_scenario_direct_close(&direct);
    return failures;
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
    const session_t *session = arg;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    char reply[TEST_BUFFER_SIZE];

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(session->port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        g_failures++;
        close(fd);
        return NULL;
    }

    for (const exchange_t *ex = session->exchanges; ex->command; ex++)
    {
        if (send(fd, ex->command, strlen(ex->command) + 1, 0) < 0 || read_reply(fd, reply, sizeof(reply)) != 0)
        {
            printf("FAIL '%s': no reply\n", ex->command);
            g_failures++;
            break;
        }
        if (strcmp(reply, ex->reply) != 0)
        {
            printf("FAIL '%s': got '%s', want '%s'\n", ex->command, reply, ex->reply);
            g_failures++;
        }
        else
        {
            printf("ok   '%s' -> '%s'\n", ex->command, reply);
        }
    }

    close(fd);
    return NULL;
}

static int g_idle_calls;

static void count_idle(void)
{
    g_idle_calls++;
}

static void *idle_client_thread(void *arg)
{
    const session_t *session = arg;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(session->port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        g_failures++;
        close(fd);
        return NULL;
    }
    usleep(100000);
    close(fd);
    return NULL;
}

/* A quiet client for 100 ms with a 10 ms idle interval: the idle callback runs
 * with no message arriving, at least 5 times. */
static int run_idle_session(int port)
{
    session_t session = { port, NULL };
    pthread_t thread;

    if (socket_start(port, 0, TEST_BUFFER_SIZE) < 0)
    {
        printf("FAIL socket_start on port %i\n", port);
        return 1;
    }

    g_idle_calls = 0;
    socket_set_idle_cb(count_idle);
    socket_set_idle_interval(10);

    pthread_create(&thread, NULL, idle_client_thread, &session);
    socket_run(0);
    pthread_join(thread, NULL);

    socket_set_idle_interval(0);
    socket_set_idle_cb(NULL);
    socket_finish();

    if (g_idle_calls >= 5)
    {
        printf("ok   %d idle calls in 100 ms without a message\n", g_idle_calls);
    }
    else
    {
        printf("FAIL %d idle calls in 100 ms without a message, want at least 5\n", g_idle_calls);
        g_failures++;
    }

    return 0;
}

static int connect_port(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* the line mod-host's effects.c sends for an output port, terminator included */
static const char g_output_set_line[] = "output_set 3 level 0.250000";
static const char g_remote_pages_changed_line[] = "remote_pages_changed 3";

static void *feedback_client_thread(void *arg)
{
    const session_t *session = arg;
    int fd = connect_port(session->port);
    int fbfd = connect_port(session->port + 1);
    char reply[TEST_BUFFER_SIZE];
    char line[TEST_BUFFER_SIZE];
    size_t got = 0;

    if (fd < 0 || fbfd < 0)
    {
        g_failures++;
        goto out;
    }

    if (send(fd, "monitor_output 3 level", 23, 0) < 0 || read_reply(fd, reply, sizeof(reply)) != 0
        || strcmp(reply, "resp 1") != 0)
    {
        printf("FAIL 'monitor_output 3 level' with a feedback client\n");
        g_failures++;
        goto out;
    }

    while (got < sizeof(line))
    {
        ssize_t n = recv(fbfd, line + got, 1, 0);
        if (n <= 0)
            break;
        if (line[got++] == '\0')
            break;
    }

    if (got == sizeof(g_output_set_line) && memcmp(line, g_output_set_line, got) == 0)
    {
        printf("ok   feedback '%s'\n", line);
    }
    else
    {
        printf("FAIL feedback: got %zu bytes '%.*s', want '%s'\n", got, (int)got, line, g_output_set_line);
        g_failures++;
    }

    got = 0;
    while (got < sizeof(line))
    {
        ssize_t n = recv(fbfd, line + got, 1, 0);
        if (n <= 0)
            break;
        if (line[got++] == '\0')
            break;
    }

    if (got == sizeof(g_remote_pages_changed_line) && memcmp(line, g_remote_pages_changed_line, got) == 0)
    {
        printf("ok   feedback '%s'\n", line);
    }
    else
    {
        printf("FAIL feedback: got %zu bytes '%.*s', want '%s'\n", got, (int)got, line,
               g_remote_pages_changed_line);
        g_failures++;
    }

out:
    if (fbfd >= 0)
        close(fbfd);
    if (fd >= 0)
        close(fd);
    return NULL;
}

/* monitor_output reaches the backend, which reports the port's value with host_dispatch_output_set() */
static int run_feedback_session(int port)
{
    session_t session = { port, NULL };
    pthread_t thread;

    if (host_dispatch_output_set(3, "level", 0.25f) != -1)
    {
        printf("FAIL output_set without a feedback client\n");
        g_failures++;
    }
    if (host_dispatch_remote_pages_changed(3) != -1)
    {
        printf("FAIL remote_pages_changed without a feedback client\n");
        g_failures++;
    }

    if (socket_start(port, port + 1, TEST_BUFFER_SIZE) < 0)
    {
        printf("FAIL socket_start on ports %i and %i\n", port, port + 1);
        return 1;
    }

    socket_set_receive_cb(protocol_parse);
    host_dispatch_register(&g_empty_backend);
    host_dispatch_register_monitor_output(fake_monitor_output);

    pthread_create(&thread, NULL, feedback_client_thread, &session);
    socket_run(0);
    pthread_join(thread, NULL);

    socket_finish();
    protocol_remove_commands();

    return 0;
}

static int run_session(int port, const host_backend_t *backend, int (*monitor_output)(int, const char *),
                       const host_plugin_info_t *info, const exchange_t *exchanges, const char *expected_calls)
{
    session_t session = { port, exchanges };
    pthread_t thread;

    g_calls[0] = '\0';

    if (socket_start(port, 0, TEST_BUFFER_SIZE) < 0)
    {
        printf("FAIL socket_start on port %i\n", port);
        return 1;
    }

    socket_set_receive_cb(protocol_parse);
    host_dispatch_register(backend);
    host_dispatch_register_monitor_output(monitor_output);
    host_dispatch_register_plugin_info(info);
    protocol_add_command("licensee %i", host_dispatch_unsupported_cb);

    pthread_create(&thread, NULL, client_thread, &session);
    socket_run(0);
    pthread_join(thread, NULL);

    socket_finish();
    protocol_remove_commands();

    if (strcmp(g_calls, expected_calls) != 0)
    {
        printf("FAIL calls: got '%s', want '%s'\n", g_calls, expected_calls);
        g_failures++;
    }

    return 0;
}

/* the pin path words (PHD_PIN_PATH_ESCAPE) and the uri of an add (phd_add_uri), as protocol.h declares them */
static int check_words(void)
{
    static const char *const paths[][2] = {
        { "Surge XT.clap", "Surge%20XT.clap" },
        { "a,b=c\td\r\ne", "a%2Cb%3Dc%09d%0D%0Ae" },
        { "100% \"q\" \\", "100%25%20%22q%22%20%5C" },
        { "sub/dir/\xc3\xb1.so", "sub/dir/\xc3\xb1.so" },
        { "plain.so", "plain.so" },
    };
    static const char *const bad[] = { "a%", "a%2", "a%zz", "a%00b" };
    char word[256], back[256], joined[64];
    char *add[] = { "add", "clap:/x/Surge", "XT.clap#id", "5", "chan" };
    char *one[] = { "add", "clap:/x/plain.clap#id", "5" };
    char *lone[] = { "add", "clap:/x/no", "id", "5" };
    unsigned n, next;
    int failures = 0;
    const char *uri;

    for (n = 0; n < sizeof(paths) / sizeof(paths[0]); n++)
    {
        if (phd_pin_path_encode(paths[n][0], word, sizeof(word)) != 0 || strcmp(word, paths[n][1]))
            failures++, printf("FAIL encode %s: got %s, want %s\n", paths[n][0], word, paths[n][1]);
        else if (strpbrk(word, PHD_PIN_PATH_EXCLUDED "\"\\"))
            failures++, printf("FAIL encode %s: %s is not one bare word\n", paths[n][0], word);
        if (phd_pin_path_decode(word, back, sizeof(back)) != 0 || strcmp(back, paths[n][0]))
            failures++, printf("FAIL decode %s: got %s\n", word, back);
    }
    if (phd_pin_path_decode("Surge%20xt%2c.clap", back, sizeof(back)) != 0 || strcmp(back, "Surge xt,.clap"))
        failures++, printf("FAIL decode lowercase hex: got %s\n", back);
    for (n = 0; n < sizeof(bad) / sizeof(bad[0]); n++)
        if (phd_pin_path_decode(bad[n], back, sizeof(back)) == 0)
            failures++, printf("FAIL decode %s: a stray or NUL escape is taken\n", bad[n]);
    if (phd_pin_path_encode("Surge XT.clap", word, 15) == 0)
        failures++, printf("FAIL encode: a word longer than its buffer is written\n");
    uri = phd_add_uri(add, 5, joined, sizeof(joined), &next);
    if (!uri || strcmp(uri, "clap:/x/Surge XT.clap#id") || next != 3)
        failures++, printf("FAIL add uri split at a space: %s, next %u\n", uri ? uri : "(null)", next);
    uri = phd_add_uri(one, 3, joined, sizeof(joined), &next);
    if (uri != one[1] || next != 2)
        failures++, printf("FAIL add uri of one word: %s, next %u\n", uri ? uri : "(null)", next);
    uri = phd_add_uri(lone, 4, joined, sizeof(joined), &next);
    if (uri != lone[1] || next != 2)
        failures++, printf("FAIL add uri with no '#': %s, next %u\n", uri ? uri : "(null)", next);
    uri = phd_add_uri(add, 5, joined, 10, &next);
    if (uri != add[1] || next != 2)
        failures++, printf("FAIL add uri too long for its buffer: %s, next %u\n", uri ? uri : "(null)", next);
    /* no display: Qt is given offscreen; a platform the user set, or a display, is left alone */
    unsetenv("DISPLAY"), unsetenv("WAYLAND_DISPLAY"), unsetenv("QT_QPA_PLATFORM");
    phd_headless_qt();
    if (!getenv("QT_QPA_PLATFORM") || strcmp(getenv("QT_QPA_PLATFORM"), PHD_HEADLESS_QT_PLATFORM))
        failures++, printf("FAIL no display: QT_QPA_PLATFORM is %s\n", getenv("QT_QPA_PLATFORM") ? getenv("QT_QPA_PLATFORM") : "unset");
    setenv("QT_QPA_PLATFORM", "xcb", 1);
    phd_headless_qt();
    if (strcmp(getenv("QT_QPA_PLATFORM"), "xcb"))
        failures++, printf("FAIL the user's QT_QPA_PLATFORM is overwritten\n");
    unsetenv("QT_QPA_PLATFORM"), setenv("WAYLAND_DISPLAY", "wayland-0", 1);
    phd_headless_qt();
    if (getenv("QT_QPA_PLATFORM"))
        failures++, printf("FAIL a display is there and QT_QPA_PLATFORM is set\n");
    unsetenv("WAYLAND_DISPLAY");
    printf("%s\n", failures ? "pin path words FAILED" : "pin path words ok");
    return failures;
}

int main(void)
{
    const char *env = getenv("PROTOCOL_TEST_PORT");
    int port = env ? atoi(env) : TEST_PORT_DEFAULT;

    g_failures += check_words();
    if (run_session(port, &g_fake_backend, fake_monitor_output, &g_fake_info, g_fake_exchanges, g_fake_calls) != 0)
        return 1;
    if (run_session(port, &g_empty_backend, NULL, &g_empty_info, g_empty_exchanges, "") != 0)
        return 1;
    if (run_feedback_session(port) != 0)
        return 1;
    if (run_idle_session(port) != 0)
        return 1;

    const char *scenarios = getenv("HOST_SCENARIOS");
    if (!scenarios)
        scenarios = "tests/host-scenarios.txt";
    if (run_scenarios(scenarios, &g_ref_backend) != 0)
        g_failures++;
    if (run_scenarios(scenarios, &g_ref_bare_backend) != 0)
        g_failures++;
    g_failures += run_window_verbs();

    printf("%s\n", g_failures == 0 ? "protocol test ok" : "protocol test FAILED");
    return g_failures == 0 ? 0 : 1;
}
