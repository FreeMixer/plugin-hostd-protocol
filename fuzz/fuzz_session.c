/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Sessions: hello and request tags (doc/protocol.md, Sessions) through protocol_parse(), over a small table of
   commands whose callbacks answer by their word count, and the library's own hello. An input is one session, a command
   a line, run in order; then a new connection starts. Properties: no crash, no leak; a session not granted tags answers
   a line that starts with "@" "not found", as mod-host does; a session granted tags answers a line "@<tag> <rest>"
   with "@<tag> " and the answer to <rest>; hello answers the protocol's version and, of the extensions it names, the
   ones the library knows, once each and in the declared order, or -902 for a version below 1 and changes nothing then;
   a session granted errors follows a refusal "resp <n>" alone with the code the protocol declares for n, and any
   other answer, or any answer in a session not granted errors, goes out as the callback wrote it; a session granted
   seq numbers each feedback line from 1 after the hello that granted it, one more each, and no other session numbers
   one; param_get_many answers every value the backend's param_get gives, in order, or the first refusal alone;
   param_set_many leaves every value as it was when it refuses the line, and sets none when a pair is refused as it is
   checked; and a new connection starts with nothing granted. The harness keeps its own model of the grant: it sends a line that
   holds "hello" only in the plain shape "hello <word> ...", which it reads itself. */
#include "fuzz.h"
#include "../src/session.h"
#include "../src/reply-words.h"
#include "../include/plugin-hostd/protocol.h"
#include "../include/host-dispatch.h"
#include <stdio.h>

static const char *const g_extensions[] = OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_INIT;
#define EXTENSION_COUNT (sizeof(g_extensions) / sizeof(g_extensions[0]))

/* the model: what the session was granted, by the harness's own reading of each hello */
static int g_tags, g_errors, g_seq;
/* the number the model gives the last feedback line of a session granted seq */
static unsigned long g_seq_last;

/* what a callback of the harness answered last, before the library wrote it out; "" for none */
static char g_written[256];

static void count_cb(proto_t *proto)
{
    protocol_response_int((int)proto->list_count, proto);
    snprintf(g_written, sizeof(g_written), "%s", proto->response);
}

/* refuse <n> [<word>]: resp <n>, and the word after it when one is given */
static void refuse_cb(proto_t *proto)
{
    char reply[256];
    snprintf(reply, sizeof(reply), "resp %d%s%.64s", atoi(proto->list[1]), proto->list_count > 2 ? " " : "",
             proto->list_count > 2 ? proto->list[2] : "");
    protocol_response(reply, proto);
    snprintf(g_written, sizeof(g_written), "%s", reply);
}

/* A feedback line goes out as it was written, after "#<n> " in a session granted seq, n one more than the line before
   it, 1 for the first after the hello that granted seq. */
__attribute__((noinline)) static void check_seq(const char *got, const char *want)
{
    /* mutant: src/protocol.c:227 `__atomic_add_fetch(&g_seq, 1, __ATOMIC_SEQ_CST)` -> `__atomic_add_fetch(&g_seq, 2, __ATOMIC_SEQ_CST)`
       input: seq */
    if (strcmp(got, want) != 0)
        abort();
}

/* emit <n>: a feedback line, then resp 0 */
static void emit_cb(proto_t *proto)
{
    char line[64], want[128];
    snprintf(line, sizeof(line), "remote_pages_changed %d", atoi(proto->list[1]));
    if (g_seq)
        snprintf(want, sizeof(want), "#%lu %s", ++g_seq_last, line);
    else
        snprintf(want, sizeof(want), "%s", line);
    socket_send_feedback(line);
    check_seq(fuzz_feedback, want);
    protocol_response_int(0, proto);
    snprintf(g_written, sizeof(g_written), "%s", proto->response);
}

/* the backend's param_get, which param_get_many reads: a value by the symbol's length, -103 for a symbol that starts
   with 'x', -3 for an instance above 8; every call is recorded */
#define MANY_MAX 64
static struct { int resp; float value; } g_calls[MANY_MAX];
static unsigned g_ncalls;

/* the values param_set set, by symbol; every other value is the symbol's length over 4 */
#define STORE_MAX 16
static struct { char name[32]; float value; } g_store[STORE_MAX];
static unsigned g_nstore, g_sets;

static float *stored(const char *symbol, int add)
{
    /* a symbol longer than a name the store holds is never stored: it reads as its length over 4 */
    if (strlen(symbol) >= sizeof(g_store[0].name))
        return NULL;
    for (unsigned i = 0; i < g_nstore; i++)
        if (strcmp(g_store[i].name, symbol) == 0)
            return &g_store[i].value;
    if (!add || g_nstore == STORE_MAX)
        return NULL;
    snprintf(g_store[g_nstore].name, sizeof(g_store[g_nstore].name), "%s", symbol);
    return &g_store[g_nstore++].value;
}

/* param_set refuses what param_get refuses, and a value above 1000 as it sets it */
static int b_param_set(int instance, const char *symbol, float value)
{
    int resp = instance > 8 ? -3 : symbol[0] == 'x' ? -103 : value > 1000.0f ? -207 : 0;
    float *p;
    g_sets++;
    if (resp == 0 && (p = stored(symbol, 1)) != NULL)
        *p = value;
    return resp;
}

static int b_param_get(int instance, const char *symbol, float *value)
{
    int resp = instance > 8 ? -3 : symbol[0] == 'x' ? -103 : 0;
    float *p = stored(symbol, 0);
    *value = p ? *p : (float)strlen(symbol) / 4.0f;
    if (g_ncalls < MANY_MAX)
    {
        g_calls[g_ncalls].resp = resp;
        g_calls[g_ncalls].value = *value;
    }
    g_ncalls++;
    return resp;
}

static const host_backend_t g_backend = { .param_set = b_param_set, .param_get = b_param_get };

/* param_get_many answers "resp 0" and each value its backend gave, in the order of the calls, when every call
   answered one; else the code of the call that refused, which is the last one made, alone. */
__attribute__((noinline)) static void check_many(const char *got, size_t len, const char *want)
{
    /* mutant: src/host-dispatch.c:190 `if (resp < SUCCESS)` -> `if (resp < -100)`
       input: many */
    if (len != strlen(want) + 1 || memcmp(got, want, len) != 0)
        abort();
}

/* param_set_many is all or nothing: a line it refuses leaves every value as it was, and a line refused as its pairs are
   checked sets no value at all. */
__attribute__((noinline)) static void check_set_many(int refused, int unchanged, int checked_refused, unsigned sets)
{
    /* mutant: src/host-dispatch.c:238 `if (checked < SUCCESS)` -> `if (0)`
       input: setmany */
    if ((refused && !unchanged) || (checked_refused && sets != 0))
        abort();
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    static const char *const table[] = { "add %s %i ...", "remove %i", "param_set %i %s %f", "param_get %i %s",
                                         "cpu_load", "state_save ..." };
    (void)argc; (void)argv;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        protocol_add_command(table[i], count_cb);
    protocol_add_command("refuse %i ...", refuse_cb);
    protocol_add_command("emit %i", emit_cb);
    /* the dispatcher's verbs after the table's, so only those the table has not (param_get_many) are its own */
    host_dispatch_register(&g_backend);
    protocol_add_hello();
    return 0;
}

/* the answer protocol_parse() gives line, with its NUL, into out; its length, 0 for none */
static size_t parse(const char *line, char *out, size_t size)
{
    msg_t msg;
    char *copy = strdup(line);
    /* not a property: the harness's own allocation failed */
    if (!copy)
        abort();
    fuzz_reply_clear();
    g_written[0] = '\0';
    g_ncalls = 0;
    msg.sender_id = FUZZ_SENDER;
    msg.data = copy;
    msg.data_size = strlen(copy);
    protocol_parse(&msg);
    free(copy);
    size_t n = fuzz_reply_len < size ? fuzz_reply_len : size;
    memcpy(out, fuzz_reply, n);
    return n;
}

/* a character of a tag, as grammar/core.abnf's tag-char has it */
static int tag_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.';
}

/* the length of the tag a line starts with, "@" and one tag-char or more ended by a space or the line's end; 0 for none */
static size_t tag_of(const char *line)
{
    size_t n = 1;
    if (line[0] != '@')
        return 0;
    while (tag_char(line[n]))
        n++;
    return n > 1 && (line[n] == ' ' || line[n] == '\0') ? n : 0;
}

/* A line that starts with "@" and carries no tag the session reads (none was granted, or the word is no tag) names a
   verb no host has, and is answered "not found" with its NUL, as mod-host answers it. */
__attribute__((noinline)) static void check_untagged(const char *got, size_t len)
{
    /* mutant: src/protocol.c:312 `granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS)` -> `1`
       input: mixed */
    if (len != sizeof("not found") || memcmp(got, "not found", len) != 0)
        abort();
}

/* A session granted tags answers a line "@<tag> <rest>" with "@<tag> ", then the answer the session gives <rest>
   alone, or "not found" for an empty <rest>, which alone is not answered. */
__attribute__((noinline)) static void check_tagged(const char *got, size_t len, const char *want, size_t want_len)
{
    /* mutant: src/protocol.c:275 `line = MALLOC(tag_size + 1 + size);` -> `line = NULL;`
       input: tags */
    if (len != want_len || memcmp(got, want, len) != 0)
        abort();
}

/* hello answers "resp 0 <version>" and the extensions asked for that the library knows, once each, in the declared
   order, for a version of 1 or more; "resp -902" for any other version. */
__attribute__((noinline)) static void check_hello(const char *got, size_t len, const char *want)
{
    /* mutant: src/protocol.c:195 `want[i] = 0;` -> `want[i] = 1;`
       input: mixed */
    if (len != strlen(want) + 1 || memcmp(got, want, len) != 0)
        abort();
}

/* A new connection starts with nothing granted, whatever the one before it was granted: a tagged line is no tag. */
__attribute__((noinline)) static void check_fresh(const char *got, size_t len)
{
    /* mutant: src/protocol.c:160 `memset(g_granted, 0, sizeof(g_granted));` -> `(void)g_granted;`
       input: tags */
    if (len != sizeof("not found") || memcmp(got, "not found", len) != 0)
        abort();
}

/* An answer a callback wrote goes out as it wrote it, but in a session granted errors a refusal "resp <n>" alone, with
   a code the protocol declares for n, is followed by that code. */
__attribute__((noinline)) static void check_errors(const char *got, size_t len, const char *want)
{
    /* mutant: src/protocol.c:439 `granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_ERRORS)` -> `1`
       input: errors */
    if (len != strlen(want) + 1 || memcmp(got, want, len) != 0)
        abort();
}

/* what an answer written as bare must go out as: with its error word in a session granted errors */
static void with_word(const char *bare, char *want, size_t size)
{
    char again[32];
    int resp;
    snprintf(want, size, "%s", bare);
    if (!g_errors || sscanf(bare, "resp %d", &resp) != 1 || resp >= 0)
        return;
    snprintf(again, sizeof(again), "resp %d", resp);
    if (strcmp(again, bare) != 0)
        return;
    for (size_t i = 0; i < sizeof(g_reply_words) / sizeof(g_reply_words[0]); i++)
        if (g_reply_words[i].resp == resp)
            snprintf(want, size, "%s %s", bare, g_reply_words[i].code);
}

/* what param_get_many must answer for the calls its backend recorded; 0 when they cannot say */
static int many_model(char *want, size_t size)
{
    size_t pos;
    if (g_ncalls == 0 || g_ncalls > MANY_MAX)
        return 0;
    for (unsigned i = 0; i < g_ncalls; i++)
        if (g_calls[i].resp < 0)
        {
            /* a refusal is the last call; its error word is the errors property's */
            if (i != g_ncalls - 1)
                snprintf(want, size, "a call after a refusal");
            else
                snprintf(want, size, "resp %d", g_calls[i].resp);
            return 1;
        }
    pos = (size_t)snprintf(want, size, "resp 0");
    for (unsigned i = 0; i < g_ncalls && pos < size; i++)
        pos += (size_t)snprintf(want + pos, size - pos, " %.04f", g_calls[i].value);
    return 1;
}

/* the answer of an untagged line, held to what its callback wrote */
static size_t parse_untagged(const char *line, char *out, size_t size)
{
    char want[4096];
    if (strncmp(line, "param_set_many ", 15) == 0)
    {
        /* the values before the line, and the param_set calls it makes */
        __typeof__(g_store) before;
        unsigned nbefore = g_nstore;
        memcpy(before, g_store, sizeof(before));
        g_sets = 0;
        size_t len = parse(line, out, size);
        int checked_refused = 0;
        for (unsigned i = 0; i < g_ncalls && i < MANY_MAX; i++)
            checked_refused |= g_calls[i].resp < 0;
        /* every value reads as before: one set back to what it held may now be stored with that same value */
        int unchanged = 1;
        for (unsigned i = 0; i < g_nstore; i++)
        {
            float was = (float)strlen(g_store[i].name) / 4.0f;
            for (unsigned j = 0; j < nbefore; j++)
                if (strcmp(before[j].name, g_store[i].name) == 0)
                    was = before[j].value;
            /* NaN is a value a line may set, and reads as itself */
            unchanged = unchanged && (g_store[i].value == was || (g_store[i].value != g_store[i].value && was != was));
        }
        check_set_many(len > 6 && strncmp(out, "resp -", 6) == 0, unchanged, checked_refused, g_sets);
        return len;
    }
    size_t len = parse(line, out, size);
    if (strncmp(line, "param_get_many ", 15) == 0 && many_model(want, sizeof(want)))
    {
        /* the answer without an error word is param_get_many's; whether the word follows is the errors property's */
        char bare[4096], worded[4096];
        snprintf(bare, sizeof(bare), "%.*s", (int)(len ? len - 1 : 0), out);
        if (strncmp(bare, "resp -", 6) == 0 && strchr(bare + 5, ' '))
            *strchr(bare + 5, ' ') = '\0';
        check_many(bare, strlen(bare) + 1, want);
        with_word(bare, worded, sizeof(worded));
        check_errors(out, len, worded);
        return len;
    }
    if (g_written[0])
    {
        with_word(g_written, want, sizeof(want));
        check_errors(out, len, want);
    }
    return len;
}

/* the plain shape of a hello the harness reads itself: "hello", then words of letters, digits, '-', '_', '.' and '+'
   each after one space, and no "...", which the parser takes for the pattern's tail */
static int plain_hello(const char *line)
{
    if (strncmp(line, "hello ", 6) != 0 || strstr(line, "..."))
        return 0;
    for (const char *p = line + 5; *p; p++)
    {
        if (*p == ' ' && (p[1] == ' ' || p[1] == '\0'))
            return 0;
        if (*p != ' ' && *p != '+' && !tag_char(*p))
            return 0;
    }
    return 1;
}

/* what the library must answer a plain hello, and the grant after it; 0 for a refused one */
static int hello_model(const char *line, char *want, size_t size)
{
    char *copy = strdup(line + 6), *save = NULL;
    /* not a property: the harness's own allocation failed */
    if (!copy)
        abort();
    char *version = strtok_r(copy, " ", &save);
    const char *d = version + (version[0] == '+' || version[0] == '-');
    int digits = *d != '\0', nonzero = 0;
    for (; *d; d++)
    {
        digits = digits && *d >= '0' && *d <= '9';
        nonzero = nonzero || (*d >= '1' && *d <= '9');
    }
    if (!digits || !nonzero || version[0] == '-')
    {
        snprintf(want, size, "resp -902");
        free(copy);
        return 0;
    }
    int asked[EXTENSION_COUNT] = { 0 };
    for (char *w = strtok_r(NULL, " ", &save); w; w = strtok_r(NULL, " ", &save))
        for (size_t i = 0; i < EXTENSION_COUNT; i++)
            asked[i] = asked[i] || strcmp(w, g_extensions[i]) == 0;
    size_t pos = (size_t)snprintf(want, size, "resp 0 %d", PLUGIN_HOSTD_PROTOCOL_VERSION);
    for (size_t i = 0; i < EXTENSION_COUNT; i++)
        if (asked[i])
            pos += (size_t)snprintf(want + pos, size - pos, " %s", g_extensions[i]);
    g_tags = g_errors = g_seq = 0;
    g_seq_last = 0;
    for (size_t i = 0; i < EXTENSION_COUNT; i++)
    {
        if (asked[i] && strcmp(g_extensions[i], OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_SEQ) == 0)
            g_seq = 1;
        if (asked[i] && strcmp(g_extensions[i], OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS) == 0)
            g_tags = 1;
        if (asked[i] && strcmp(g_extensions[i], OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_ERRORS) == 0)
            g_errors = 1;
    }
    free(copy);
    return 1;
}

static char g_got[FUZZ_REPLY_MAX], g_rest[FUZZ_REPLY_MAX], g_want[FUZZ_REPLY_MAX];

static void run_line(const char *line)
{
    size_t len, tag;

    if (plain_hello(line))
    {
        char want[256], worded[512];
        /* the word after a refused hello's number is the errors property's: a refusal changes no grant */
        int accepted = hello_model(line, want, sizeof(want));
        len = parse(line, g_got, sizeof(g_got));
        if (accepted)
            check_hello(g_got, len, want);
        else
        {
            with_word(want, worded, sizeof(worded));
            check_errors(g_got, len, worded);
        }
        return;
    }
    /* a hello the harness does not read itself would move the grant behind its model */
    if (strstr(line, "hello"))
        return;

    tag = g_tags ? tag_of(line) : 0;
    if (!tag)
    {
        if (line[0] == '@')
            check_untagged(g_got, parse(line, g_got, sizeof(g_got)));
        else
            parse_untagged(line, g_got, sizeof(g_got));
        return;
    }

    /* the rest after the tag and its one space, answered alone first (a callback here answers by its words alone); a
       rest that starts with "@" is a verb no host has, which alone would be read as a tag of its own */
    const char *rest = line[tag] == ' ' ? line + tag + 1 : line + tag;
    size_t rest_len = rest[0] == '@' ? 0 : parse_untagged(rest, g_rest, sizeof(g_rest));
    if (rest_len == 0)
        rest_len = (size_t)snprintf(g_rest, sizeof(g_rest), "not found") + 1;
    if (tag + 1 + rest_len > sizeof(g_want))
        return;
    memcpy(g_want, line, tag);
    g_want[tag] = ' ';
    memcpy(g_want + tag + 1, g_rest, rest_len);
    len = parse(line, g_got, sizeof(g_got));
    check_tagged(g_got, len, g_want, tag + 1 + rest_len);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > FUZZ_MAX_INPUT)
        return 0;

    /* a connection: nothing granted, and the backend's values as they start */
    g_nstore = 0;
    memset(g_store, 0, sizeof(g_store));
    socket_session_start();
    g_tags = g_errors = g_seq = 0;
    g_seq_last = 0;

    char *text = fuzz_message(data, size), *save = NULL;
    for (char *line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
        run_line(line);
    free(text);

    /* the end of the session: tags dropped, a tagged line is no tag; then granted again, for the next connection */
    run_line("hello 1");
    run_line("@probe cpu_load");
    run_line("hello 1 tags");

    /* the next connection */
    socket_session_start();
    g_tags = g_errors = g_seq = 0;
    g_seq_last = 0;
    size_t len = parse("@fresh cpu_load", g_got, sizeof(g_got));
    check_fresh(g_got, len);
    return 0;
}
