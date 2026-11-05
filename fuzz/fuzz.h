/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared by the libFuzzer harnesses of this directory (fuzz/README.md). */
#ifndef FUZZ_H
#define FUZZ_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/utils.h"
#include "../include/protocol.h"

#define FUZZ_MAX_INPUT 8192

/* Where protocol_parse() answers: fuzz/socket_fuzz.c replaces src/socket.c in every harness, and its socket_send()
   appends each reply, with its NUL, to fuzz_reply. A harness empties it before an input and reads it after. */
#define FUZZ_REPLY_MAX 65536
extern char fuzz_reply[FUZZ_REPLY_MAX];
extern size_t fuzz_reply_len;
/* the last feedback line a harness sent, with the number a session granted seq puts before it */
extern char fuzz_feedback[FUZZ_REPLY_MAX];

/* the sender id to put in msg_t: any value but STDOUT_FILENO, which the parser writes to directly */
#define FUZZ_SENDER 7

static void fuzz_reply_clear(void)
{
    fuzz_reply_len = 0;
    fuzz_reply[0] = '\0';
}

/* The input as the NUL-terminated message the socket layer hands the parser (it cuts at the first NUL). */
static char *fuzz_message(const uint8_t *data, size_t size)
{
    char *copy = malloc(size + 1);
    /* not a property: the harness's own allocation failed */
    if (!copy)
        abort();
    memcpy(copy, data, size);
    copy[size] = '\0';
    return copy;
}

/* The words of a message by the protocol's quoting rule, as mod-host's parser reads them: trailing blanks go; a space
   outside quotes ends a word, and a run of them is one; a '"' opens a quoted run, in which a space belongs to the word
   and '""' stands for one '"'; the '"' that closes the run ends the word's text, and what follows it up to the next
   space is dropped; '\"' is a '"' anywhere. Then each '"' left in a word goes, and the character after it stays
   whatever it is. buf holds strlen(m) + 1 bytes and receives the words; words gets the first max of them. Answers how
   many words the message has, max or more. */
static inline unsigned fuzz_words(const char *m, char *buf, char **words, unsigned max)
{
    size_t len = strlen(m);
    unsigned n = 1;
    int quoted = 0;
    while (len > 0 && (m[len - 1] == ' ' || m[len - 1] == '\t' || m[len - 1] == '\r' || m[len - 1] == '\n'))
        len--;
    memcpy(buf, m, len);
    buf[len] = '\0';
    if (max > 0)
        words[0] = buf;
    for (char *p = buf; *p; p++)
    {
        if (*p == ' ' && !quoted)
        {
            *p = '\0';
            while (p[1] == ' ')
                p++;
            if (n < max)
                words[n] = p + 1;
            n++;
        }
        else if (*p == '\\' && p[1] == '"')
            *p++ = '"';
        else if (*p == '"' && !quoted)
            quoted = 1;
        else if (*p == '"' && p[1] == '"')
            p++;
        else if (*p == '"')
        {
            *p = '\0';
            quoted = 0;
        }
    }
    for (unsigned i = 0; i < n && i < max; i++)
        for (char *q = words[i]; *q; q++)
            if (*q == '"')
                memmove(q, q + 1, strlen(q));
    return n;
}

/* a word the parser may take for a pattern's wildcard or its "...", which match more than the word itself */
static inline int fuzz_wildcard(const char *word)
{
    return strchr(word, '%') != NULL || strstr(word, "...") != NULL;
}

#endif
