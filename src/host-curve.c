/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * The curve verbs (doc/protocol.md, Curves): curve_bind keeps, for a display of an instance's panel description, the
 * parameter that plays each role the description gives it; curve_points reads each of them from the instance as it is
 * now, with the rate the host runs the instance at, and draws the display with omx-dsp's omx_curve_points, the one
 * curve function every surface calls. The binding is what a controller told the host, kept with the instance and
 * dropped with its remove; it is never saved, and the host judges nothing of it.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <omxdsp/omx_curve.h>

#include "host-curve.h"
#include "host-dispatch.h"
#include "host-errors.h"
#include "mod-host.h"
#include "plugin-hostd/plugin_hostd_protocol_host_verbs.h"
#include "plugin-hostd/plugin_hostd_protocol_vocabulary.h"

/* mod-host's remove of every instance */
#define CURVE_REMOVE_ALL (-1)
/* the longest symbol a term names, and the most choice values a band_type's shapes give */
#define CURVE_SYMBOL_MAX 128
#define CURVE_SHAPES_MAX 64
/* the most terms one display binds: every band's five roles, and the head filters' six */
#define CURVE_TERMS_MAX (OMX_CURVE_BANDS_MAX * 5u + 6u)

typedef struct {
    enum omx_curve_roles role;
    uint32_t index;
    enum omx_curve_units unit;
    char symbol[CURVE_SYMBOL_MAX];
    uint32_t nshapes;
    signed char shapes[CURVE_SHAPES_MAX];
} curve_term_t;

typedef struct curve_binding {
    struct curve_binding *next;
    int instance;
    int display;
    enum omx_curve_displays kind;
    enum omx_curve_families family;
    uint32_t nterms;
    curve_term_t terms[CURVE_TERMS_MAX];
} curve_binding_t;

static const host_curve_t *g_curve;
static const host_backend_t *g_curve_backend;
static curve_binding_t *g_bindings;

static curve_binding_t **binding_slot(int instance, int display)
{
    curve_binding_t **b;

    for (b = &g_bindings; *b; b = &(*b)->next)
        if ((*b)->instance == instance && (*b)->display == display)
            return b;
    return b;
}

void host_curve_forget(int instance)
{
    curve_binding_t **b = &g_bindings;

    while (*b)
    {
        if (instance == CURVE_REMOVE_ALL || (*b)->instance == instance)
        {
            curve_binding_t *gone = *b;
            *b = gone->next;
            free(gone);
        }
        else
            b = &(*b)->next;
    }
}

/* a whole non-negative number of at most nine digits, the whole of [s, end) */
static int parse_index(const char *s, const char *end, uint32_t *out)
{
    uint32_t v = 0;

    if (s == end || end - s > 9)
        return -1;
    for (; s < end; s++)
    {
        if (*s < '0' || *s > '9')
            return -1;
        v = v * 10u + (uint32_t)(*s - '0');
    }
    *out = v;
    return 0;
}

static int word_in(const char *const *words, uint32_t n, const char *s, const char *end)
{
    char buf[32];
    size_t len = (size_t)(end - s);

    if (len == 0 || len >= sizeof(buf))
        return -1;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return omx_curve_word(words, n, buf);
}

static int symbol_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

/* one term, <role>[.<index>]=<symbol>[:<unit>][@<shape>|<shape>...]: 0, or the code that refuses it */
static int parse_term(const char *w, curve_term_t *t)
{
    const char *eq = strchr(w, '='), *dot, *p, *sym, *end;
    int v;

    memset(t, 0, sizeof(*t));
    if (!eq)
        return ERR_INVALID_OPERATION;
    for (dot = w; dot < eq && *dot != '.'; dot++)
        ;
    v = word_in(OMX_CURVE_ROLE_WORDS, OMX_CURVE_ROLES_COUNT, w, dot);
    if (v < 0)
        return ERR_INVALID_OPERATION;
    t->role = (enum omx_curve_roles)v;
    if (dot < eq && parse_index(dot + 1, eq, &t->index) != 0)
        return ERR_INVALID_OPERATION;
    /* the symbol: an optional leading ':' (a host's own control, :bypass), then its characters */
    sym = p = eq + 1;
    if (*p == ':')
        p++;
    while (symbol_char(*p))
        p++;
    if (p == sym || (*sym == ':' && p == sym + 1) || (size_t)(p - sym) >= CURVE_SYMBOL_MAX)
        return ERR_INVALID_OPERATION;
    memcpy(t->symbol, sym, (size_t)(p - sym));
    t->unit = OMX_CURVE_UNITS_NONE;
    if (*p == ':')
    {
        for (end = ++p; *end && *end != '@'; end++)
            ;
        if (end == p)
            return ERR_INVALID_OPERATION;
        v = word_in(OMX_CURVE_UNIT_WORDS, OMX_CURVE_UNITS_COUNT, p, end);
        if (v < 0)
            return OMX_REPLY_PLUGIN_HOSTD_CURVE_UNIT_REFUSED_RESP;
        t->unit = (enum omx_curve_units)v;
        p = end;
    }
    if (*p == '@')
    {
        if (t->role != OMX_CURVE_ROLES_BAND_TYPE && t->role != OMX_CURVE_ROLES_BAND_FREQ)
            return ERR_INVALID_OPERATION;
        for (p++;; p = end + 1)
        {
            for (end = p; *end && *end != '|'; end++)
                ;
            v = word_in(OMX_CURVE_SHAPE_WORDS, OMX_CURVE_SHAPES_COUNT, p, end);
            if (v < 0 || t->nshapes >= CURVE_SHAPES_MAX)
                return ERR_INVALID_OPERATION;
            t->shapes[t->nshapes++] = (signed char)v;
            if (!*end)
                break;
        }
        if (t->role == OMX_CURVE_ROLES_BAND_FREQ && t->nshapes != 1)
            return ERR_INVALID_OPERATION;
    }
    else if (*p)
        return ERR_INVALID_OPERATION;
    if (!omx_curve_unit_accepted(t->role, t->unit))
        return OMX_REPLY_PLUGIN_HOSTD_CURVE_UNIT_REFUSED_RESP;
    return SUCCESS;
}

/* a whole number, the whole word */
static int whole(const char *w, int *out)
{
    char *end;
    long v = strtol(w, &end, 10);

    if (*w == '\0' || *end != '\0' || v < -2147483647L || v > 2147483647L)
        return -1;
    *out = (int)v;
    return 0;
}

/* the instance's rate, or the code that refuses it: -3 for an instance the host does not have */
static int rate_of(int instance, double *rate)
{
    if (!g_curve || !g_curve->sample_rate)
        return ERR_INVALID_OPERATION;
    return g_curve->sample_rate(instance, rate);
}

/* curve_bind <instance> <display> clear | <kind> <family> <term> ... */
static void curve_bind_cb(proto_t *proto)
{
    int instance, display;
    curve_term_t *terms = NULL;
    uint32_t nterms = 0, i, j;
    double rate;
    int code, kind, family;
    curve_binding_t **slot, *b;

    if (whole(proto->list[1], &instance) != 0 || whole(proto->list[2], &display) != 0)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }
    if ((code = rate_of(instance, &rate)) < 0)
    {
        protocol_response_int(code, proto);
        return;
    }
    if (display < 0 || !g_curve_backend || !g_curve_backend->param_get)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }
    slot = binding_slot(instance, display);
    if (strcmp(proto->list[3], "clear") == 0)
    {
        if (proto->list_count != 4)
        {
            protocol_response_int(ERR_INVALID_OPERATION, proto);
            return;
        }
        if (*slot)
        {
            b = *slot;
            *slot = b->next;
            free(b);
        }
        protocol_response_int(SUCCESS, proto);
        return;
    }
    kind = omx_curve_word(OMX_CURVE_DISPLAY_WORDS, OMX_CURVE_DISPLAYS_COUNT, proto->list[3]);
    if (kind < 0 || proto->list_count < 6)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }
    family = omx_curve_word(OMX_CURVE_FAMILY_WORDS, OMX_CURVE_FAMILIES_COUNT, proto->list[4]);
    if (family < 0 || (kind == OMX_CURVE_DISPLAYS_EQ_RESPONSE) != (family == OMX_CURVE_FAMILIES_EQ))
    {
        protocol_response_int(OMX_REPLY_PLUGIN_HOSTD_CURVE_KIND_UNKNOWN_RESP, proto);
        return;
    }
    nterms = proto->list_count - 5;
    if (!(terms = calloc(nterms, sizeof(*terms))))
    {
        protocol_response_int(ERR_MEMORY_ALLOCATION, proto);
        return;
    }
    /* every term checked before any is kept */
    for (i = 0; i < nterms; i++)
    {
        float value;

        if ((code = parse_term(proto->list[5 + i], &terms[i])) == SUCCESS)
            code = g_curve_backend->param_get(instance, terms[i].symbol, &value);
        if (code < 0)
        {
            free(terms);
            protocol_response_int(code, proto);
            return;
        }
    }
    b = *slot;
    if (b && (b->kind != (enum omx_curve_displays)kind || b->family != (enum omx_curve_families)family))
        b->nterms = 0;
    if (!b)
    {
        if (!(b = calloc(1, sizeof(*b))))
        {
            free(terms);
            protocol_response_int(ERR_MEMORY_ALLOCATION, proto);
            return;
        }
        b->instance = instance;
        b->display = display;
        *slot = b;
    }
    b->kind = (enum omx_curve_displays)kind;
    b->family = (enum omx_curve_families)family;
    for (i = 0; i < nterms; i++)
    {
        for (j = 0; j < b->nterms; j++)
            if (b->terms[j].role == terms[i].role && b->terms[j].index == terms[i].index)
                break;
        if (j == b->nterms && b->nterms == CURVE_TERMS_MAX)
        {
            /* no room: a binding past every band's roles; what was kept before this line stays */
            free(terms);
            protocol_response_int(OMX_REPLY_PLUGIN_HOSTD_CURVE_POINTS_RANGE_RESP, proto);
            return;
        }
        b->terms[j] = terms[i];
        if (j == b->nterms)
            b->nterms++;
    }
    free(terms);
    protocol_response_int(SUCCESS, proto);
}

/* x to four significant digits, written with no exponent */
static int format_x(char *buf, size_t size, double x)
{
    int decimals;

    if (x == 0.0)
        return snprintf(buf, size, "0");
    decimals = 3 - (int)floor(log10(fabs(x)));
    if (decimals < 0)
        x = round(x / pow(10.0, -decimals)) * pow(10.0, -decimals), decimals = 0;
    return snprintf(buf, size, "%.*f", decimals, x);
}

int host_curve_code(int omx_code)
{
    switch (omx_code)
    {
    case OMX_CURVE_OK: return SUCCESS;
    case OMX_CURVE_KIND_UNKNOWN: return OMX_REPLY_PLUGIN_HOSTD_CURVE_KIND_UNKNOWN_RESP;
    case OMX_CURVE_TERM_MISSING: return OMX_REPLY_PLUGIN_HOSTD_CURVE_TERM_MISSING_RESP;
    case OMX_CURVE_UNIT_REFUSED: return OMX_REPLY_PLUGIN_HOSTD_CURVE_UNIT_REFUSED_RESP;
    case OMX_CURVE_POINTS_RANGE: return OMX_REPLY_PLUGIN_HOSTD_CURVE_POINTS_RANGE_RESP;
    case OMX_CURVE_ROOM: return ERR_MEMORY_ALLOCATION;
    default: return ERR_INVALID_OPERATION; /* OMX_CURVE_VALUE_NOT_FINITE: a value or a rate that is not a number */
    }
}

/* curve_points <instance> <display> <points> */
static void curve_points_cb(proto_t *proto)
{
    int instance, display, points;
    struct omx_curve_term rqt[CURVE_TERMS_MAX];
    struct omx_curve_request rq;
    double rate, x[OMX_CURVE_POINTS_MAX], y[OMX_CURVE_POINTS_MAX];
    char buffer[SOCKET_MSG_BUFFER_SIZE * 4], xs[32];
    curve_binding_t *b;
    size_t pos;
    uint32_t i, n;
    int code;

    if (whole(proto->list[1], &instance) != 0 || whole(proto->list[2], &display) != 0 ||
        whole(proto->list[3], &points) != 0)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }
    if ((code = rate_of(instance, &rate)) < 0)
    {
        protocol_response_int(code, proto);
        return;
    }
    if (points < 2 || points > (int)OMX_CURVE_POINTS_MAX)
    {
        protocol_response_int(OMX_REPLY_PLUGIN_HOSTD_CURVE_POINTS_RANGE_RESP, proto);
        return;
    }
    b = *binding_slot(instance, display);
    if (!b || b->nterms == 0)
    {
        protocol_response_int(OMX_REPLY_PLUGIN_HOSTD_CURVE_TERM_MISSING_RESP, proto);
        return;
    }
    if (!g_curve_backend || !g_curve_backend->param_get)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }
    for (i = 0; i < b->nterms; i++)
    {
        const curve_term_t *t = &b->terms[i];
        float value;

        if ((code = g_curve_backend->param_get(instance, t->symbol, &value)) < 0)
        {
            protocol_response_int(code, proto);
            return;
        }
        rqt[i].role = t->role;
        rqt[i].index = t->index;
        rqt[i].value = value;
        rqt[i].unit = t->unit;
        rqt[i].shape = OMX_CURVE_NO_SHAPE;
        /* a host binds a plugin's controls, which carry no origin (spec §7.6: the strip's EQ is the console's) */
        rqt[i].origin = OMX_CURVE_ORIGINS_USER;
        if (t->role == OMX_CURVE_ROLES_BAND_TYPE && t->nshapes)
        {
            /* the shape the choice value selects; a value past the list is a choice with no shape */
            const double c = nearbyint((double)value);
            rqt[i].shape = c >= 0.0 && c < (double)t->nshapes ? t->shapes[(uint32_t)c] : (int)OMX_CURVE_SHAPES_UNSHAPED;
        }
        else if (t->nshapes)
            rqt[i].shape = t->shapes[0];
    }
    memset(&rq, 0, sizeof(rq));
    rq.kind = b->kind;
    rq.family = b->family;
    rq.sample_rate = rate;
    rq.points = (uint32_t)points;
    rq.terms = rqt;
    rq.nterms = b->nterms;
    n = omx_curve_points(&rq, x, y, OMX_CURVE_POINTS_MAX, &code);
    if (code != OMX_CURVE_OK)
    {
        protocol_response_int(host_curve_code(code), proto);
        return;
    }
    pos = (size_t)snprintf(buffer, sizeof(buffer), "resp 0 %u", n);
    for (i = 0; i < n && pos < sizeof(buffer); i++)
    {
        format_x(xs, sizeof(xs), x[i]);
        /* -0.00 is written 0.00: the sign of a zero is no part of the curve */
        pos += (size_t)snprintf(buffer + pos, sizeof(buffer) - pos, " %s,%.2f", xs, fabs(y[i]) < 0.005 ? 0.0 : y[i]);
    }
    if (pos >= sizeof(buffer))
        protocol_response_int(ERR_MEMORY_ALLOCATION, proto);
    else
        protocol_response(buffer, proto);
}

void host_dispatch_register_curve(const host_curve_t *curve)
{
    g_curve = curve;
    protocol_add_command(OMX_VERB_CURVE_BIND_FMT, curve_bind_cb);
    protocol_add_command(OMX_VERB_CURVE_POINTS_FMT, curve_points_cb);
}

void host_curve_set_backend(const host_backend_t *backend)
{
    g_curve_backend = backend;
}
