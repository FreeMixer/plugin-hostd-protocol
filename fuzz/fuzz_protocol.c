/* The command parser alone: protocol_parse() over a table of commands that has every shape the real one
   has (fixed words, %i/%s/%f wildcards, a trailing "..."), with callbacks that only count what they were
   given. Properties: no crash, no leak, a callback only ever runs with the argument count its pattern
   allows (exactly, or at least the words before the "..." for a variable pattern) and for a command whose first
   word is its pattern's, a command that names a verb of the table runs its callback exactly when its word
   count is in the row's range and is otherwise answered "few arguments" or "many arguments", any other command
   is answered "not found", and every protocol error code has its message. */
#include "fuzz.h"
#include <stdio.h>

static int g_called;

static const struct { const char *pattern, *verb; uint32_t min, max; } g_table[] = {
    { "add %s %i ...",          "add",        3, UINT32_MAX },
    { "remove %i",              "remove",     2, 2 },
    { "param_set %i %s %f",     "param_set",  4, 4 },
    { "param_get %i %s",        "param_get",  3, 3 },
    { "connect %s %s",          "connect",    3, 3 },
    { "quit",                   "quit",       1, 1 },
    { "cpu_load",               "cpu_load",   1, 1 },
    { "midi_map %i %s %i %i",   "midi_map",   5, 5 },
    { "patch_set %i %s ...",    "patch_set",  3, UINT32_MAX },
    { "state_save ...",         "state_save", 1, UINT32_MAX },
};

/* The parser hands a callback only a command with the argument count its pattern allows: fewer is "few arguments", more
   is "many arguments", and neither reaches the callback. g_table counts the words before a "...", so a variable pattern
   is called with that many at least. */
__attribute__((noinline)) static void check_arity(const proto_t *proto, unsigned row)
{
    /* mutant: src/protocol.c:220 `proto.list_count < (g_commands[i].count - variable_arguments)` -> `0`
       input: extra-14 */
    if (proto->list_count < g_table[row].min || proto->list_count > g_table[row].max)
        abort();
}

/* The callback that runs is the one of the command the input names: its first word is the pattern's. */
__attribute__((noinline)) static void check_callback(const proto_t *proto, unsigned row)
{
    /* mutant: src/protocol.c:256 `g_commands[index].callback(&proto);` -> `g_commands[0].callback(&proto);`
       input: extra-9
       mutant: src/protocol.c:190 ` && (j == 0 || match > 0)` -> ``
       input: extra-23 */
    if (proto->list_count == 0 || strcmp(proto->list[0], g_table[row].verb) != 0)
        abort();
}

/* one callback per row, so each checks the count its own pattern allows */
static void on_command(proto_t *proto, unsigned row)
{
    g_called++;
    check_callback(proto, row);
    check_arity(proto, row);
    protocol_response_int((int)proto->list_count, proto);
}

#define CB(n) static void cb##n(proto_t *proto) { on_command(proto, n); }
CB(0) CB(1) CB(2) CB(3) CB(4) CB(5) CB(6) CB(7) CB(8) CB(9)
static void (*const g_cbs[])(proto_t *) = { cb0, cb1, cb2, cb3, cb4, cb5, cb6, cb7, cb8, cb9 };

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc; (void)argv;
    for (unsigned i = 0; i < sizeof(g_table) / sizeof(g_table[0]); i++)
        protocol_add_command(g_table[i].pattern, g_cbs[i]);
    return 0;
}

/* A command that names a verb of the table with a word count in its row's range runs its callback once, and any other
   command runs none. */
__attribute__((noinline)) static void check_called(int exact, int expect_called)
{
    /* mutant: src/protocol.c:226 `match == proto.list_count || variable_arguments` -> `0`
       input: extra-1
       mutant: src/utils.c:203 `quote = 0;` -> ``
       input: quoted-words */
    if (exact && g_called != expect_called)
        abort();
}

/* The parser answers once, with its NUL: the callback's "resp <words>", "few arguments" below the row's minimum, "many
   arguments" above its maximum, or "not found" for a verb outside the table. */
__attribute__((noinline)) static void check_reply(int exact, const char *expect_reply)
{
    /* mutant: src/protocol.c:221 `result = PROTOCOL_FEW_ARGUMENTS;` -> `result = PROTOCOL_MANY_ARGUMENTS;`
       input: extra-14
       mutant: src/protocol.c:224 `result = PROTOCOL_MANY_ARGUMENTS;` -> `result = PROTOCOL_FEW_ARGUMENTS;`
       input: extra-16 */
    if (exact && (fuzz_reply_len != strlen(expect_reply) + 1 || strcmp(fuzz_reply, expect_reply) != 0))
        abort();
}

/* Each of the parser's four error codes has its message, with the text the protocol documents. */
__attribute__((noinline)) static void check_messages(void)
{
    static const char *const texts[] = { "not found", "many arguments", "few arguments", "invalid argument" };
    for (int code = -1; code >= -4; code--)
    {
        const char *message = protocol_error_message(code);
        /* mutant: src/protocol.c:342 `g_error_messages[-code-1]` -> `g_error_messages[0]`
           input: extra-1
           mutant: src/protocol.c:339 `code >= 0` -> `code >= -1`
           input: extra-1 */
        if (message == NULL || strcmp(message, texts[-code - 1]) != 0)
            abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > FUZZ_MAX_INPUT)
        return 0;

    g_called = 0;
    fuzz_reply_clear();

    msg_t msg;
    msg.data = fuzz_message(data, size);
    msg.data_size = strlen(msg.data);
    msg.sender_id = FUZZ_SENDER;

    /* what the input must get, from its words by the quoting rule (fuzz_words) and the table. Left to the callbacks' own
       checks: a message with a word that holds a "%" or a "...", which the parser may take for a pattern's wildcard. */
    char *buf = malloc(msg.data_size + 1);
    char **word = malloc((msg.data_size + 2) * sizeof(*word));
    /* not a property: the harness's own allocation failed */
    if (!buf || !word)
        abort();
    unsigned words = fuzz_words(msg.data, buf, word, (unsigned)msg.data_size + 2);
    int exact = 1;
    for (unsigned i = 0; i < words; i++)
        exact = exact && !fuzz_wildcard(word[i]);
    int expect_called = 0;
    char expect_reply[32] = "not found";
    for (unsigned row = 0; row < sizeof(g_table) / sizeof(g_table[0]); row++)
    {
        if (strcmp(word[0], g_table[row].verb) != 0)
            continue;
        if (words < g_table[row].min)
            strcpy(expect_reply, "few arguments");
        else if (words > g_table[row].max)
            strcpy(expect_reply, "many arguments");
        else
        {
            expect_called = 1;
            snprintf(expect_reply, sizeof(expect_reply), "resp %u", words);
        }
    }

    free(buf);
    free(word);

    protocol_parse(&msg);
    free(msg.data);

    check_messages();
    check_called(exact, expect_called);
    check_reply(exact, expect_reply);
    return 0;
}
