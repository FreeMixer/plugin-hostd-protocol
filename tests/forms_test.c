/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * A verb of more than one form (the plugin-hostd protocol's verb table: preset_show <uri> is mod-host's,
 * preset_show <instance> <key> plugin-hostd's) runs the callback of the form its arguments fit, whichever was
 * registered first; the types choose between forms of one count; a line no form takes is refused as the first form
 * refuses it, as mod-host refuses a verb of one form. Through the library's own parser and a socket, as a host meets it.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#include <stdio.h>
#include <string.h>

#include <host-backend.h>
#include <host-dispatch.h>
#include <host-errors.h>
#include <host-scenario.h>
#include <protocol.h>

static int g_failures;
static char g_calls[64];

static void call(const char *name)
{
    strncat(g_calls, g_calls[0] ? "," : "", sizeof(g_calls) - strlen(g_calls) - 1);
    strncat(g_calls, name, sizeof(g_calls) - strlen(g_calls) - 1);
}

/* mod-host's form: the preset's state, with no resp before it */
static void show_uri(proto_t *proto)
{
    char state[128];

    call("uri");
    snprintf(state, sizeof(state), "@prefix p: <%s> .", proto->list[1]);
    protocol_response(state, proto);
}

/* plugin-hostd's form: one line of JSON */
static void show_instance(proto_t *proto)
{
    char reply[128];

    call("instance");
    snprintf(reply, sizeof(reply), "resp 0 {\"instance\":%s,\"key\":\"%s\"}", proto->list[1], proto->list[2]);
    protocol_response(reply, proto);
}

static void typed_int(proto_t *proto)
{
    call("int");
    protocol_response_int(SUCCESS, proto);
}

static void typed_word(proto_t *proto)
{
    call("word");
    protocol_response_int(SUCCESS, proto);
}

static void expect(host_scenario_direct_t *direct, const char *what, const char *command, const char *reply,
                   const char *calls)
{
    char got[256];

    g_calls[0] = '\0';
    if (host_scenario_direct_exchange(direct, command, got, sizeof(got)) != 0)
    {
        printf("FAIL %s '%s': no reply\n", what, command);
        g_failures++;
        return;
    }
    if (strcmp(got, reply) != 0 || strcmp(g_calls, calls) != 0)
    {
        printf("FAIL %s '%s': got '%s' calling [%s], want '%s' calling [%s]\n", what, command, got, g_calls, reply,
               calls);
        g_failures++;
        return;
    }
    printf("ok   %s '%s' -> '%s' [%s]\n", what, command, got, g_calls);
}

static const host_backend_t g_backend = { 0 };

static void run(const char *order)
{
    host_scenario_direct_t direct;

    /* the two forms, in the order given, before the library's own preset_show (which answers -902) */
    if (!strcmp(order, "mod-host's first"))
    {
        protocol_add_command("preset_show %s", show_uri);
        protocol_add_command("preset_show %i %s", show_instance);
    }
    else
    {
        protocol_add_command("preset_show %i %s", show_instance);
        protocol_add_command("preset_show %s", show_uri);
    }
    protocol_add_command("form_test %i", typed_int);
    protocol_add_command("form_test %s", typed_word);
    if (host_scenario_direct_open(&direct, &g_backend) != 0)
    {
        printf("FAIL socketpair\n");
        g_failures++;
        return;
    }
    printf("-- %s\n", order);
    expect(&direct, "mod-host's form", "preset_show urn:preset:warm", "@prefix p: <urn:preset:warm> .", "uri");
    expect(&direct, "mod-host's form, quoted", "preset_show \"urn:preset:a b\"", "@prefix p: <urn:preset:a b> .", "uri");
    expect(&direct, "plugin-hostd's form", "preset_show 3 warm", "resp 0 {\"instance\":3,\"key\":\"warm\"}", "instance");
    /* no form takes it: the first form's refusal, as mod-host refuses a verb of one form */
    expect(&direct, "no form", "preset_show", "few arguments", "");
    expect(&direct, "no form", "preset_show 3 warm extra", "many arguments", "");
    /* one count, two forms: the types choose */
    expect(&direct, "the int form", "form_test 7", "resp 0", "int");
    expect(&direct, "the word form", "form_test seven", "resp 0", "word");
    host_scenario_direct_close(&direct);
}

int main(void)
{
    run("mod-host's first");
    run("plugin-hostd's first");
    printf("%s\n", g_failures == 0 ? "forms ok" : "forms FAILED");
    return g_failures == 0 ? 0 : 1;
}
