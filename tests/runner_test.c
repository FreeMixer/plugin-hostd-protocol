/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * The scenario runner and "not found": a declared verb not required of the host under test (hello, which no host is
 * required to answer; worker_list, which only plugin-hostd is) is a verb that host does not serve, and the lines it
 * guards are skipped; a declared verb required of the host fails, and does not skip what it guards. With no host
 * named, a verb required of any host fails. A host answers from a fixed list here, so nothing but the runner is tested.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <host-scenario.h>

static int g_failures;
static int g_sent_guarded;

/* the host: it knows neither worker_list nor hello */
static int exchange(void *ctx, const char *command, char *reply, size_t size)
{
    (void)ctx;
    if (strcmp(command, "remove 3") == 0 || strcmp(command, "remove 4") == 0)
        g_sent_guarded++;
    snprintf(reply, size, "%s", strncmp(command, "remove", 6) == 0 ? "resp 0" : "not found");
    return 0;
}

static void run(const char *path, const char *host, int want_failures, int want_guarded)
{
    host_scenario_list_t list;
    int failures;

    if (host_scenario_load(path, NULL, 0, &list) <= 0)
    {
        printf("FAIL cannot load %s\n", path);
        g_failures++;
        return;
    }
    host_scenario_set_host(host);
    g_sent_guarded = 0;
    failures = host_scenario_run(&list, exchange, NULL, NULL);
    host_scenario_free(&list);
    if (failures != want_failures || g_sent_guarded != want_guarded)
    {
        printf("FAIL host %s: %d failures and %d guarded lines sent, want %d and %d\n", host ? host : "(none)",
               failures, g_sent_guarded, want_failures, want_guarded);
        g_failures++;
        return;
    }
    printf("ok   host %s: %d failures, %d guarded lines sent\n", host ? host : "(none)", failures, g_sent_guarded);
}

int main(void)
{
    char path[] = "/tmp/runner-test-XXXXXX";
    int fd = mkstemp(path);
    static const char scenarios[] =
        "worker_list => resp 0 | not found\n"
        "@worker_list remove 3 => resp 0\n"
        "hello 1 => resp 0 1 | not found\n"
        "@hello remove 4 => resp 0\n";

    if (fd < 0 || write(fd, scenarios, sizeof(scenarios) - 1) != (ssize_t)(sizeof(scenarios) - 1))
    {
        printf("FAIL cannot write %s\n", path);
        return 1;
    }
    close(fd);

    /* plugin-hostd is required to answer worker_list: a failure, and its guarded line is sent */
    run(path, "plugin-hostd", 1, 1);
    /* mod-host is not: worker_list and hello are verbs it does not serve, and both guarded lines are skipped */
    run(path, "mod-host", 0, 0);
    /* no host named: required of one host is required */
    run(path, NULL, 1, 1);
    unlink(path);

    printf("%s\n", g_failures == 0 ? "runner ok" : "runner FAILED");
    return g_failures == 0 ? 0 : 1;
}
