/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * The curve verbs (doc/protocol.md, Curves), through the library's own server on a real TCP socket, over a backend
 * whose parameters the test sets:
 *   - curve_points answers omx_curve_points over the values param_get reads and the rate the host gives, bit for bit
 *     as the reply writes them, for an EQ and a compressor; a value changed is read on the next curve_points;
 *   - a 32-band binding sent in three lines answers as the one request it makes;
 *   - a term of a role and index already bound replaces it, a line of another kind or family starts the binding again,
 *     clear drops it, and remove drops every binding of its instance (remove -1 every binding);
 *   - every term is checked before any is kept: a line refused keeps nothing of it;
 *   - each refusal by its code, and a malformed line of each shape -902;
 *   - a host that does not register the curve verbs answers both -902.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <omxdsp/omx_curve.h>

#include <host-backend.h>
#include <host-dispatch.h>
#include <host-errors.h>
#include <protocol.h>
#include <socket.h>

#include "test-client.h"

#define CURVE_PORT_DEFAULT 15596
#define CURVE_RATE 96000.0
#define REPLY_MAX 8192

static int g_failures;
static volatile int g_running = 1;

/* instance 3 and instance 4 each have the parameters p0 .. p199; instance 3 runs at 96 kHz, 4 at 48 kHz */
#define PARAMS 200
static float g_params[2][PARAMS];

static float *param_of(int instance, const char *symbol, int *code)
{
    int n;
    char tail;

    *code = instance != 3 && instance != 4 ? ERR_INSTANCE_NON_EXISTS : SUCCESS;
    if (*code == SUCCESS && sscanf(symbol, "p%d%c", &n, &tail) == 1 && n >= 0 && n < PARAMS)
        return &g_params[instance - 3][n];
    if (*code == SUCCESS)
        *code = ERR_LV2_INVALID_PARAM_SYMBOL;
    return NULL;
}

static int b_param_get(int instance, const char *symbol, float *value)
{
    int code;
    float *p = param_of(instance, symbol, &code);

    if (p)
        *value = *p;
    return code;
}

static int b_param_set(int instance, const char *symbol, float value)
{
    int code;
    float *p = param_of(instance, symbol, &code);

    if (p)
        *p = value;
    return code;
}

static int g_removable = 1;
static int b_remove(int instance)
{
    return (instance == 3 || instance == 4 || instance == -1) && g_removable ? SUCCESS : ERR_INSTANCE_NON_EXISTS;
}

static int b_sample_rate(int instance, double *rate)
{
    if (instance != 3 && instance != 4)
        return ERR_INSTANCE_NON_EXISTS;
    *rate = instance == 3 ? CURVE_RATE : 48000.0;
    return SUCCESS;
}

static const host_backend_t g_backend = { .remove = b_remove, .param_set = b_param_set, .param_get = b_param_get };
static const host_curve_t g_curve = { .sample_rate = b_sample_rate };

static void *server_thread(void *arg)
{
    (void)arg;
    while (g_running)
        socket_run(0);
    return NULL;
}

static void expect(conn_t *c, const char *what, const char *command, const char *want)
{
    static char got[REPLY_MAX];

    exchange(c, command, got, sizeof(got));
    if (strcmp(got, want) != 0)
    {
        printf("FAIL %s '%.120s': got '%.200s', want '%.200s'\n", what, command, got, want);
        g_failures++;
        return;
    }
    printf("ok   %s '%.80s' -> '%.80s'\n", what, command, got);
}

/* the reply curve_points must give: omx_curve_points over the request, written as the verb writes it */
static void reply_of(const struct omx_curve_request *rq, char *out, size_t size)
{
    double x[OMX_CURVE_POINTS_MAX], y[OMX_CURVE_POINTS_MAX];
    int code = -1;
    uint32_t n = omx_curve_points(rq, x, y, OMX_CURVE_POINTS_MAX, &code);
    size_t pos;

    if (code != OMX_CURVE_OK)
    {
        snprintf(out, size, "omx_curve_points code %d", code);
        return;
    }
    pos = (size_t)snprintf(out, size, "resp 0 %u", n);
    for (uint32_t i = 0; i < n; i++)
    {
        char xs[32];
        int decimals = x[i] == 0.0 ? -1 : 3 - (int)floor(log10(fabs(x[i])));
        double xv = x[i];
        if (decimals < 0 && x[i] != 0.0)
            xv = round(x[i] / pow(10.0, -decimals)) * pow(10.0, -decimals), decimals = 0;
        if (x[i] == 0.0)
            snprintf(xs, sizeof(xs), "0");
        else
            snprintf(xs, sizeof(xs), "%.*f", decimals, xv);
        pos += (size_t)snprintf(out + pos, size - pos, " %s,%.2f", xs, fabs(y[i]) < 0.005 ? 0.0 : y[i]);
    }
}

static void expect_curve(conn_t *c, const char *what, const char *command, const struct omx_curve_request *rq)
{
    static char want[REPLY_MAX];

    reply_of(rq, want, sizeof(want));
    expect(c, what, command, want);
}

static void set(int instance, int n, float v)
{
    g_params[instance - 3][n] = v;
}

int main(void)
{
    const char *env = getenv("CURVE_PORT");
    static char line[REPLY_MAX], got[REPLY_MAX];
    pthread_t thread;
    conn_t c;
    size_t pos;
    int b, k;

    setvbuf(stdout, NULL, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);
    g_port = env ? atoi(env) : CURVE_PORT_DEFAULT;
    if (socket_start(g_port, g_port + 1, 8192) != 0)
    {
        printf("FAIL socket_start on ports %d and %d\n", g_port, g_port + 1);
        return 1;
    }
    socket_set_receive_cb(protocol_parse);
    pthread_create(&thread, NULL, server_thread, NULL);

    /* -- a host that does not register the curve verbs */
    protocol_remove_commands();
    host_dispatch_register(&g_backend);
    host_dispatch_register_unsupported();
    c = open_session();
    expect(&c, "no curve hook", "curve_bind 3 0 transfer_curve dynamics threshold=p0:db ratio=p1:ratio", "resp -902");
    expect(&c, "no curve hook", "curve_points 3 0 8", "resp -902");
    close_session(&c);

    protocol_remove_commands();
    host_dispatch_register(&g_backend);
    host_dispatch_register_curve(&g_curve);
    host_dispatch_register_unsupported();
    c = open_session();

    /* -- a compressor: the reply is omx_curve_points over the values param_get reads, at the instance's rate */
    set(3, 0, -18.5f), set(3, 1, 4.0f), set(3, 2, 6.0f), set(3, 3, 2.5f);
    expect(&c, "bind a compressor", "curve_bind 3 0 transfer_curve dynamics threshold=p0:dbfs ratio=p1:ratio knee=p2:db makeup=p3:db", "resp 0");
    {
        struct omx_curve_term t[] = {
            {OMX_CURVE_ROLES_THRESHOLD, 0, -18.5f, OMX_CURVE_UNITS_DBFS, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_RATIO, 0, 4.0f, OMX_CURVE_UNITS_RATIO, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_KNEE, 0, 6.0f, OMX_CURVE_UNITS_DB, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_MAKEUP, 0, 2.5f, OMX_CURVE_UNITS_DB, OMX_CURVE_NO_SHAPE},
        };
        struct omx_curve_request rq = {OMX_CURVE_DISPLAYS_TRANSFER_CURVE, OMX_CURVE_FAMILIES_DYNAMICS, CURVE_RATE, 41u, 0, 0, 0, t, 4u};
        expect_curve(&c, "compressor points", "curve_points 3 0 41", &rq);
        /* a value changed on the instance is the one the next curve_points reads */
        set(3, 1, 2.0f);
        t[1].value = 2.0f;
        expect_curve(&c, "the ratio as the instance holds it now", "curve_points 3 0 41", &rq);
        rq.points = OMX_CURVE_POINTS_MAX;
        expect_curve(&c, "the most points", "curve_points 3 0 256", &rq);
        /* a term of a bound role replaces it */
        set(3, 4, -30.0f);
        expect(&c, "rebind the threshold", "curve_bind 3 0 transfer_curve dynamics threshold=p4:db", "resp 0");
        t[0].value = -30.0f;
        t[0].unit = OMX_CURVE_UNITS_DB;
        rq.points = 9u;
        expect_curve(&c, "the threshold replaced, the rest kept", "curve_points 3 0 9", &rq);
    }

    /* -- an EQ with a band_type: the shape its value selects, the rate of instance 4 (48 kHz) */
    set(4, 0, 2.0f), set(4, 1, 1000.0f), set(4, 2, 2.0f), set(4, 3, 1.0f), set(4, 4, 1.0f);
    set(4, 5, 0.12f), set(4, 6, 24.0f);
    expect(&c, "bind an EQ",
           "curve_bind 4 1 eq_response eq band_type.0=p0@off|bell|lowshelf|highshelf band_freq.0=p1:hz band_gain.0=p2:coef "
           "band_q.0=p3:oct band_on.0=p4 hpf_freq=p5:khz hpf_slope=p6:db", "resp 0");
    {
        struct omx_curve_term t[] = {
            {OMX_CURVE_ROLES_BAND_TYPE, 0, 2.0f, OMX_CURVE_UNITS_NONE, OMX_CURVE_SHAPES_LOWSHELF},
            {OMX_CURVE_ROLES_BAND_FREQ, 0, 1000.0f, OMX_CURVE_UNITS_HZ, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_BAND_GAIN, 0, 2.0f, OMX_CURVE_UNITS_COEF, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_BAND_Q, 0, 1.0f, OMX_CURVE_UNITS_OCT, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_BAND_ON, 0, 1.0f, OMX_CURVE_UNITS_NONE, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_HPF_FREQ, 0, 0.12f, OMX_CURVE_UNITS_KHZ, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_HPF_SLOPE, 0, 24.0f, OMX_CURVE_UNITS_DB, OMX_CURVE_NO_SHAPE},
        };
        struct omx_curve_request rq = {OMX_CURVE_DISPLAYS_EQ_RESPONSE, OMX_CURVE_FAMILIES_EQ, 48000.0, 64u, 0, 0, 0, t, 7u};
        expect_curve(&c, "EQ points", "curve_points 4 1 64", &rq);
        set(4, 0, 1.0f);
        t[0].value = 1.0f;
        t[0].shape = OMX_CURVE_SHAPES_BELL;
        expect_curve(&c, "the band_type's value picks the bell", "curve_points 4 1 64", &rq);
        /* 256 points with three sections drawn: 256 - 9 grid points fit, one more does not */
        rq.points = OMX_CURVE_POINTS_MAX - 9u;
        expect_curve(&c, "247 grid points and nine anchors", "curve_points 4 1 247", &rq);
        expect(&c, "248 grid points and nine anchors", "curve_points 4 1 248", "resp -539");
        set(4, 0, 0.0f);
        t[0].value = 0.0f;
        t[0].shape = OMX_CURVE_SHAPES_OFF;
        rq.points = 248u;
        expect_curve(&c, "the band switched off by its type: the high-pass's six anchors only", "curve_points 4 1 248", &rq);
        set(4, 0, 7.0f);
        expect(&c, "a choice value past the shapes", "curve_points 4 1 64", "resp -536");
        set(4, 0, 1.0f);
        set(4, 6, 18.0f);
        expect(&c, "an 18 dB/oct head filter", "curve_points 4 1 64", "resp -536");
        set(4, 6, 24.0f);
        set(4, 2, NAN);
        expect(&c, "a gain that is not a number", "curve_points 4 1 64", "resp -902");
        set(4, 2, 2.0f);
    }

    /* -- a 32-band binding in three lines answers as the one request it makes */
    {
        static struct omx_curve_term t[160];
        struct omx_curve_request rq = {OMX_CURVE_DISPLAYS_EQ_RESPONSE, OMX_CURVE_FAMILIES_EQ, CURVE_RATE, 32u, 0, 0, 0, t, 0u};
        for (b = 0; b < 32; b++)
        {
            set(3, 10 + 4 * b, 25.0f * powf(2.0f, (float)b / 3.2f));
            set(3, 11 + 4 * b, (float)((b * 7) % 25 - 12));
            set(3, 12 + 4 * b, 0.5f + (float)(b % 6));
            set(3, 13 + 4 * b, (float)(b % 3));
        }
        for (k = 0; k < 3; k++)
        {
            pos = (size_t)snprintf(line, sizeof(line), "curve_bind 3 2 eq_response eq");
            for (b = k * 11; b < 32 && b < (k + 1) * 11; b++)
                pos += (size_t)snprintf(line + pos, sizeof(line) - pos,
                                        " band_freq.%d=p%d:hz band_gain.%d=p%d:db band_q.%d=p%d band_type.%d=p%d@bell|lowshelf|highshelf",
                                        b, 10 + 4 * b, b, 11 + 4 * b, b, 12 + 4 * b, b, 13 + 4 * b);
            expect(&c, "a third of a 32-band binding", line, "resp 0");
        }
        for (b = 0; b < 32; b++)
        {
            const int sh[3] = {OMX_CURVE_SHAPES_BELL, OMX_CURVE_SHAPES_LOWSHELF, OMX_CURVE_SHAPES_HIGHSHELF};
            t[rq.nterms++] = (struct omx_curve_term){OMX_CURVE_ROLES_BAND_FREQ, (uint32_t)b, g_params[0][10 + 4 * b], OMX_CURVE_UNITS_HZ, OMX_CURVE_NO_SHAPE};
            t[rq.nterms++] = (struct omx_curve_term){OMX_CURVE_ROLES_BAND_GAIN, (uint32_t)b, g_params[0][11 + 4 * b], OMX_CURVE_UNITS_DB, OMX_CURVE_NO_SHAPE};
            t[rq.nterms++] = (struct omx_curve_term){OMX_CURVE_ROLES_BAND_Q, (uint32_t)b, g_params[0][12 + 4 * b], OMX_CURVE_UNITS_NONE, OMX_CURVE_NO_SHAPE};
            t[rq.nterms++] = (struct omx_curve_term){OMX_CURVE_ROLES_BAND_TYPE, (uint32_t)b, g_params[0][13 + 4 * b], OMX_CURVE_UNITS_NONE, sh[b % 3]};
        }
        expect_curve(&c, "a 32-band EQ, bound in three lines", "curve_points 3 2 32", &rq);
        expect(&c, "32 bands: 256 - 96 grid points fit, one more does not", "curve_points 3 2 161", "resp -539");
        /* the compressor of display 0 is still there beside it */
        exchange(&c, "curve_points 3 0 9", got, sizeof(got));
        if (strncmp(got, "resp 0 9 ", 9) != 0)
            printf("FAIL another display's binding: got '%.80s'\n", got), g_failures++;
        else
            printf("ok   another display's binding is kept beside it\n");
    }

    /* -- a line of another kind or family starts the binding again; clear drops it */
    expect(&c, "a gate where the compressor was", "curve_bind 3 0 transfer_curve gate threshold=p4:db range=p5:db", "resp 0");
    expect(&c, "the compressor's ratio is not the gate's", "curve_points 3 0 9", "resp -537");
    set(3, 7, 3.0f);
    expect(&c, "the gate's ratio", "curve_bind 3 0 transfer_curve gate ratio=p7:coef", "resp 0");
    exchange(&c, "curve_points 3 0 9", got, sizeof(got));
    if (strncmp(got, "resp 0 9 -80.00,", 16) != 0)
        printf("FAIL the gate draws: got '%.80s'\n", got), g_failures++;
    else
        printf("ok   the gate draws once its terms are bound: '%.60s'\n", got);
    expect(&c, "clear", "curve_bind 3 0 clear", "resp 0");
    expect(&c, "cleared", "curve_points 3 0 9", "resp -537");
    expect(&c, "clear an unbound display", "curve_bind 3 5 clear", "resp 0");

    /* -- every term checked before any is kept */
    expect(&c, "bind", "curve_bind 3 0 transfer_curve dynamics threshold=p0:db ratio=p1:ratio", "resp 0");
    expect(&c, "a refused term in the line", "curve_bind 3 0 transfer_curve dynamics threshold=p4:db ratio=nope:ratio", "resp -103");
    set(3, 0, -10.0f), set(3, 1, 4.0f);
    {
        struct omx_curve_term t[] = {
            {OMX_CURVE_ROLES_THRESHOLD, 0, -10.0f, OMX_CURVE_UNITS_DB, OMX_CURVE_NO_SHAPE},
            {OMX_CURVE_ROLES_RATIO, 0, 4.0f, OMX_CURVE_UNITS_RATIO, OMX_CURVE_NO_SHAPE},
        };
        struct omx_curve_request rq = {OMX_CURVE_DISPLAYS_TRANSFER_CURVE, OMX_CURVE_FAMILIES_DYNAMICS, CURVE_RATE, 5u, 0, 0, 0, t, 2u};
        expect_curve(&c, "the refused line kept nothing: p0 is still the threshold", "curve_points 3 0 5", &rq);
    }

    /* -- each refusal by its code */
    expect(&c, "no such instance", "curve_bind 9 0 transfer_curve dynamics threshold=p0:db", "resp -3");
    expect(&c, "no such instance", "curve_points 9 0 9", "resp -3");
    expect(&c, "no such instance", "curve_bind 9 0 clear", "resp -3");
    expect(&c, "no such symbol", "curve_bind 3 0 transfer_curve dynamics threshold=nope:db", "resp -103");
    expect(&c, "a family with no closed form", "curve_bind 3 0 transfer_curve reverb threshold=p0:db", "resp -536");
    expect(&c, "an EQ family on a transfer curve", "curve_bind 3 0 transfer_curve eq threshold=p0:db", "resp -536");
    expect(&c, "a dynamics family on an EQ response", "curve_bind 3 0 eq_response dynamics band_freq=p0:hz", "resp -536");
    expect(&c, "a unit the role does not take", "curve_bind 3 0 transfer_curve dynamics threshold=p0:hz", "resp -538");
    expect(&c, "a unit no role takes", "curve_bind 3 0 transfer_curve dynamics threshold=p0:furlong", "resp -538");
    expect(&c, "no unit where the role needs one", "curve_bind 3 0 transfer_curve dynamics threshold=p0", "resp -538");
    expect(&c, "no binding", "curve_points 3 7 9", "resp -537");
    expect(&c, "one point", "curve_points 3 0 1", "resp -539");
    expect(&c, "257 points", "curve_points 3 0 257", "resp -539");
    /* a malformed line of each shape */
    expect(&c, "no kind", "curve_bind 3 0", "few arguments");
    expect(&c, "a kind the grammar does not take", "curve_bind 3 0 bar_graph dynamics threshold=p0:db", "resp -902");
    expect(&c, "a kind and no term", "curve_bind 3 0 transfer_curve dynamics", "resp -902");
    expect(&c, "clear with more", "curve_bind 3 0 clear threshold=p0:db", "resp -902");
    expect(&c, "a term with no =", "curve_bind 3 0 transfer_curve dynamics threshold", "resp -902");
    expect(&c, "a role the curve does not read", "curve_bind 3 0 transfer_curve dynamics attack=p0:ms", "resp -902");
    expect(&c, "an index that is not a number", "curve_bind 3 0 eq_response eq band_freq.x=p0:hz", "resp -902");
    expect(&c, "an empty index", "curve_bind 3 0 eq_response eq band_freq.=p0:hz", "resp -902");
    expect(&c, "no symbol", "curve_bind 3 0 transfer_curve dynamics threshold=", "resp -902");
    expect(&c, "a host's own control is a symbol: :db with no unit", "curve_bind 3 0 transfer_curve dynamics threshold=:db", "resp -538");
    expect(&c, "a host's own control the instance does not have", "curve_bind 3 0 transfer_curve dynamics threshold=:db:db", "resp -103");
    expect(&c, "an empty unit", "curve_bind 3 0 transfer_curve dynamics threshold=p0:", "resp -902");
    expect(&c, "a shape on a role with none", "curve_bind 3 0 transfer_curve dynamics threshold=p0:db@bell", "resp -902");
    expect(&c, "two fixed shapes", "curve_bind 3 0 eq_response eq band_freq.0=p0:hz@bell|notch", "resp -902");
    expect(&c, "a shape the grammar does not take", "curve_bind 3 0 eq_response eq band_type.0=p0@bell|wobble", "resp -902");
    expect(&c, "an empty shape", "curve_bind 3 0 eq_response eq band_type.0=p0@bell||notch", "resp -902");
    expect(&c, "a character no symbol has", "curve_bind 3 0 transfer_curve dynamics threshold=p0/x:db", "resp -902");
    expect(&c, "points that are no number", "curve_points 3 0 many", "resp -902");
    expect(&c, "an instance that is no number", "curve_bind 3x 0 transfer_curve dynamics threshold=p0:db", "resp -902");
    expect(&c, "curve_points with too many words", "curve_points 3 0 9 1", "many arguments");

    /* -- remove drops every binding of its instance */
    expect(&c, "bind 3", "curve_bind 3 0 transfer_curve dynamics threshold=p0:db ratio=p1:ratio", "resp 0");
    expect(&c, "bind 4", "curve_bind 4 0 transfer_curve dynamics threshold=p0:db ratio=p1:ratio", "resp 0");
    expect(&c, "remove 3", "remove 3", "resp 0");
    expect(&c, "3's binding went with it", "curve_points 3 0 9", "resp -537");
    expect(&c, "3's other display went with it", "curve_points 3 2 32", "resp -537");
    exchange(&c, "curve_points 4 0 9", got, sizeof(got));
    if (strncmp(got, "resp 0 9 ", 9) != 0)
        printf("FAIL 4's binding stays: got '%.80s'\n", got), g_failures++;
    else
        printf("ok   4's binding stays when 3 is removed\n");
    g_removable = 0;
    expect(&c, "a refused remove", "remove 4", "resp -3");
    exchange(&c, "curve_points 4 0 9", got, sizeof(got));
    if (strncmp(got, "resp 0 9 ", 9) != 0)
        printf("FAIL a refused remove keeps the binding: got '%.80s'\n", got), g_failures++;
    else
        printf("ok   a refused remove keeps the binding\n");
    g_removable = 1;
    expect(&c, "remove every instance", "remove -1", "resp 0");
    expect(&c, "4's binding went with remove -1", "curve_points 4 0 9", "resp -537");
    close_session(&c);

    g_running = 0;
    printf("%s: %d failure(s)\n", g_failures ? "FAIL" : "ok", g_failures);
    return g_failures ? 1 : 0;
}
