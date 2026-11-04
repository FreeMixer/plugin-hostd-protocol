/* Sessions: hello and request tags (doc/protocol.md, Sessions) through protocol_parse(), over a small table of
   commands whose callbacks answer by their word count, and the library's own hello. An input is one session, a command
   a line, run in order; then a new connection starts. Properties: no crash, no leak; a session not granted tags answers
   a line that starts with "@" "not found", as mod-host does; a session granted tags answers a line "@<tag> <rest>"
   with "@<tag> " and the answer to <rest>; hello answers the protocol's version and, of the extensions it names, the
   ones the library knows, once each and in the declared order, or -902 for a version below 1 and changes nothing then;
   and a new connection starts with nothing granted. The harness keeps its own model of the grant: it sends a line that
   holds "hello" only in the plain shape "hello <word> ...", which it reads itself. */
#include "fuzz.h"
#include "../src/session.h"
#include "../include/plugin-hostd/protocol.h"
#include <stdio.h>

static const char *const g_extensions[] = OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_INIT;
#define EXTENSION_COUNT (sizeof(g_extensions) / sizeof(g_extensions[0]))

/* the model: what the session was granted, by the harness's own reading of each hello */
static int g_tags;

static void count_cb(proto_t *proto)
{
    protocol_response_int((int)proto->list_count, proto);
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    static const char *const table[] = { "add %s %i ...", "remove %i", "param_set %i %s %f", "param_get %i %s",
                                         "cpu_load", "state_save ..." };
    (void)argc; (void)argv;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        protocol_add_command(table[i], count_cb);
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
    /* mutant: src/protocol.c:263 `granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS)` -> `1`
       input: mixed */
    if (len != sizeof("not found") || memcmp(got, "not found", len) != 0)
        abort();
}

/* A session granted tags answers a line "@<tag> <rest>" with "@<tag> ", then the answer the session gives <rest>
   alone, or "not found" for an empty <rest>, which alone is not answered. */
__attribute__((noinline)) static void check_tagged(const char *got, size_t len, const char *want, size_t want_len)
{
    /* mutant: src/protocol.c:226 `line = MALLOC(tag_size + 1 + size);` -> `line = NULL;`
       input: tags */
    if (len != want_len || memcmp(got, want, len) != 0)
        abort();
}

/* hello answers "resp 0 <version>" and the extensions asked for that the library knows, once each, in the declared
   order, for a version of 1 or more; "resp -902" for any other version. */
__attribute__((noinline)) static void check_hello(const char *got, size_t len, const char *want)
{
    /* mutant: src/protocol.c:190 `want[i] = 0;` -> `want[i] = 1;`
       input: mixed */
    if (len != strlen(want) + 1 || memcmp(got, want, len) != 0)
        abort();
}

/* A new connection starts with nothing granted, whatever the one before it was granted: a tagged line is no tag. */
__attribute__((noinline)) static void check_fresh(const char *got, size_t len)
{
    /* mutant: src/protocol.c:156 `memset(g_granted, 0, sizeof(g_granted));` -> `(void)g_granted;`
       input: tags */
    if (len != sizeof("not found") || memcmp(got, "not found", len) != 0)
        abort();
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

/* what the library must answer a plain hello, and the grant after it */
static void hello_model(const char *line, char *want, size_t size)
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
        return;
    }
    int asked[EXTENSION_COUNT] = { 0 };
    for (char *w = strtok_r(NULL, " ", &save); w; w = strtok_r(NULL, " ", &save))
        for (size_t i = 0; i < EXTENSION_COUNT; i++)
            asked[i] = asked[i] || strcmp(w, g_extensions[i]) == 0;
    size_t pos = (size_t)snprintf(want, size, "resp 0 %d", PLUGIN_HOSTD_PROTOCOL_VERSION);
    for (size_t i = 0; i < EXTENSION_COUNT; i++)
        if (asked[i])
            pos += (size_t)snprintf(want + pos, size - pos, " %s", g_extensions[i]);
    g_tags = 0;
    for (size_t i = 0; i < EXTENSION_COUNT; i++)
        if (asked[i] && strcmp(g_extensions[i], OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS) == 0)
            g_tags = 1;
    free(copy);
}

static char g_got[FUZZ_REPLY_MAX], g_rest[FUZZ_REPLY_MAX], g_want[FUZZ_REPLY_MAX];

static void run_line(const char *line)
{
    size_t len, tag;

    if (plain_hello(line))
    {
        char want[256];
        hello_model(line, want, sizeof(want));
        len = parse(line, g_got, sizeof(g_got));
        check_hello(g_got, len, want);
        return;
    }
    /* a hello the harness does not read itself would move the grant behind its model */
    if (strstr(line, "hello"))
        return;

    tag = g_tags ? tag_of(line) : 0;
    if (!tag)
    {
        len = parse(line, g_got, sizeof(g_got));
        if (line[0] == '@')
            check_untagged(g_got, len);
        return;
    }

    /* the rest after the tag and its one space, answered alone first (a callback here answers by its words alone); a
       rest that starts with "@" is a verb no host has, which alone would be read as a tag of its own */
    const char *rest = line[tag] == ' ' ? line + tag + 1 : line + tag;
    size_t rest_len = rest[0] == '@' ? 0 : parse(rest, g_rest, sizeof(g_rest));
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

    /* a connection: nothing granted */
    socket_session_start();
    g_tags = 0;

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
    g_tags = 0;
    size_t len = parse("@fresh cpu_load", g_got, sizeof(g_got));
    check_fresh(g_got, len);
    return 0;
}
