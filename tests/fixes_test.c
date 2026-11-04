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

/* The fixes this library carries past mod-host's code, each against the input that showed its bug. Built with
 * AddressSanitizer and UndefinedBehaviorSanitizer over the sources (make test-fixes), so a write past an array stops
 * the run as surely as a wrong reply fails it. make test-fixes-sabotage takes each fix out and requires a failure.
 *
 *   plugin-hostd#371  param_info of a port whose range or default is not finite: format_number() wrote past digits[20]
 *   plugin-hostd#372  a command whose first word names no verb ran the callback of the first command its later words
 *                     matched (add, for "zzz %s 5")
 *   #12               a word "..." at the "..." of a pattern was read as a fixed word, so the words after it were
 *                     refused (many arguments, for "add urn:x 5 ... c")
 *   #11               a track_info name of a C1 control character (U+0080..U+009F, C2 80..C2 9F) was taken: "no control
 *                     character" is Unicode's class Cc, U+0000..U+001F, U+007F and U+0080..U+009F */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../include/host-backend.h"
#include "../include/host-dispatch.h"
#include "../include/host-errors.h"
#include "../include/host-scenario.h"

static char g_calls[1024];
static int g_failures;

static void record(const char *call)
{
    if (g_calls[0])
        strncat(g_calls, " ", sizeof(g_calls) - strlen(g_calls) - 1);
    strncat(g_calls, call, sizeof(g_calls) - strlen(g_calls) - 1);
}

static int b_add(const char *uri, int instance, const char *client)
{
    (void)uri; (void)client;
    record("add");
    return instance;
}
static int b_connect(const char *a, const char *b)
{
    (void)a; (void)b;
    record("connect");
    return SUCCESS;
}
static int b_disconnect(const char *a, const char *b)
{
    (void)a; (void)b;
    record("disconnect");
    return SUCCESS;
}
static int b_track_info(int instance, const char *name, const char *color, const char *kind)
{
    (void)instance; (void)name; (void)color; (void)kind;
    record("track_info");
    return SUCCESS;
}
/* a port's range or default need not be finite: a range times the sample rate can overflow a float */
static int b_param_info(int instance, const char *symbol, host_param_info_t *info)
{
    (void)instance;
    record("param_info");
    info->unit = "hz";
    info->scale = "log";
    info->min = 20;
    info->max = 20000;
    info->def = 440;
    info->step = 0;
    if (strcmp(symbol, "inf") == 0)
        info->max = INFINITY;
    else if (strcmp(symbol, "-inf") == 0)
        info->min = -INFINITY;
    else if (strcmp(symbol, "nan") == 0)
        info->def = NAN;
    info->stable_symbol = "stable";
    return SUCCESS;
}

static const host_backend_t g_backend = { .add = b_add, .connect = b_connect, .disconnect = b_disconnect };
static const host_plugin_info_t g_info = { .track_info = b_track_info, .param_info = b_param_info };

static void expect(host_scenario_direct_t *direct, const char *issue, const char *command, const char *reply,
                   const char *calls)
{
    char got[256];

    g_calls[0] = '\0';
    if (host_scenario_direct_exchange(direct, command, got, sizeof(got)) != 0)
    {
        printf("FAIL %s '%s': no reply\n", issue, command);
        g_failures++;
        return;
    }
    if (strcmp(got, reply) != 0 || strcmp(g_calls, calls) != 0)
    {
        printf("FAIL %s '%s': got '%s' calling [%s], want '%s' calling [%s]\n", issue, command, got, g_calls, reply,
               calls);
        g_failures++;
        return;
    }
    printf("ok   %s '%s' -> '%s' [%s]\n", issue, command, got, g_calls);
}

int main(void)
{
    host_scenario_direct_t direct;

    host_dispatch_register_plugin_info(&g_info);
    if (host_scenario_direct_open(&direct, &g_backend) != 0)
    {
        printf("FAIL socketpair\n");
        return 1;
    }

    /* #371: the three values format_number() has no exponent to split, written as ECMAScript writes them */
    expect(&direct, "#371", "param_info 3 inf", "resp 0 hz log 20 Infinity 440 0 stable", "param_info");
    expect(&direct, "#371", "param_info 3 -inf", "resp 0 hz log -Infinity 20000 440 0 stable", "param_info");
    expect(&direct, "#371", "param_info 3 nan", "resp 0 hz log 20 20000 NaN 0 stable", "param_info");
    expect(&direct, "#371", "param_info 3 gain", "resp 0 hz log 20 20000 440 0 stable", "param_info");

    /* #372: a message runs the callback of the verb it names, or none */
    expect(&direct, "#372", "zzz %s 5", "not found", "");
    expect(&direct, "#372", "%i %s %f", "not found", "");
    expect(&direct, "#372", "connect %s %i", "resp 0", "connect");
    expect(&direct, "#372", "disconnect %s %s", "resp 0", "disconnect");
    expect(&direct, "#372", "add urn:x 5", "resp 5", "add");

    /* #12: a word "..." at the "..." of a pattern leaves the words after it to the pattern */
    expect(&direct, "#12", "add urn:x 5 ... c", "resp 5", "add");

    /* #11: a name refuses the C1 control characters as it refuses C0 and DEL; U+00A0, the first code point after them,
       and a name of no control character are taken */
    expect(&direct, "#11", "track_info 0 \302\200 -", "resp -902", "");
    expect(&direct, "#11", "track_info 0 a\302\237b -", "resp -902", "");
    expect(&direct, "#11", "track_info 0 a\177b -", "resp -902", "");
    expect(&direct, "#11", "track_info 0 \302\240 -", "resp 0", "track_info");
    expect(&direct, "#11", "track_info 0 Kick -", "resp 0", "track_info");

    host_scenario_direct_close(&direct);
    printf("%s\n", g_failures == 0 ? "fixes ok" : "fixes FAILED");
    return g_failures == 0 ? 0 : 1;
}
