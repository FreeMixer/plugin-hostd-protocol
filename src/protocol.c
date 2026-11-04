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

/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SKIP_READLINE
#include <unistd.h>
#endif

#include "protocol.h"
#include "utils.h"
#include "host-errors.h"
#include "session.h"
// the declared protocol: the protocol version, hello's line and the extensions it grants (data/verbs.json)
#include "plugin-hostd/protocol.h"


/*
************************************************************************************************************************
*           LOCAL DEFINES
************************************************************************************************************************
*/



/*
************************************************************************************************************************
*           LOCAL CONSTANTS
************************************************************************************************************************
*/

static const char *g_error_messages[] = {
    MESSAGE_COMMAND_NOT_FOUND,
    MESSAGE_MANY_ARGUMENTS,
    MESSAGE_FEW_ARGUMENTS,
    MESSAGE_INVALID_ARGUMENT
};


/*
************************************************************************************************************************
*           LOCAL DATA TYPES
************************************************************************************************************************
*/

typedef struct CMD_T {
    char* command;
    char** list;
    uint32_t count;
    void (*callback)(proto_t *proto);
} cmd_t;


/*
************************************************************************************************************************
*           LOCAL MACROS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL GLOBAL VARIABLES
************************************************************************************************************************
*/

static int g_verbose = 0;
static unsigned int g_command_count = 0;
static cmd_t g_commands[PROTOCOL_MAX_COMMANDS];

// the extensions hello grants, in the order its reply names them, and which of them this connection was granted
static const char *const g_extensions[] = OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_INIT;
#define EXTENSION_COUNT (sizeof(g_extensions) / sizeof(g_extensions[0]))
static unsigned char g_granted[EXTENSION_COUNT];


/*
************************************************************************************************************************
*           LOCAL FUNCTION PROTOTYPES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           LOCAL FUNCTIONS
************************************************************************************************************************
*/

static int is_wildcard(const char *str)
{
    if (str && strchr(str, '%') != NULL) return 1;
    return 0;
}

/* the words of a message are of the types of a command's wildcards: %i a whole number, %f a number; any other
   wildcard takes any word. A verb of one form is read as mod-host reads it, by count alone; the types only choose
   between the forms of a verb whose arguments fit more than one */
static int typed(const cmd_t *command, const proto_t *proto)
{
    uint32_t j;
    char *end;

    for (j = 1; j < proto->list_count && j < command->count; j++)
    {
        const char *w = command->list[j], *word = proto->list[j];

        if (!strcmp(w, "%i"))
        {
            (void)strtol(word, &end, 10);
            if (*word == '\0' || *end != '\0') return 0;
        }
        else if (!strcmp(w, "%f"))
        {
            (void)strtod(word, &end);
            if (*word == '\0' || *end != '\0') return 0;
        }
    }
    return 1;
}


/* a new session: nothing granted, every line answered as mod-host answers it */
static void session_start(void)
{
    memset(g_granted, 0, sizeof(g_granted));
}

static int granted(const char *extension)
{
    size_t i;

    for (i = 0; i < EXTENSION_COUNT; i++)
        if (strcmp(g_extensions[i], extension) == 0)
            return g_granted[i];
    return 0;
}

/* hello <version> [<extension> ...]: the version a whole number of 1 or more, else -902 and nothing changes. The
   extensions asked for that the library knows are granted, every other one is not; the reply names the protocol's
   version and the ones granted, once each, in the order of the declared list */
static void hello_cb(proto_t *proto)
{
    unsigned char want[EXTENSION_COUNT];
    char *end, *buffer;
    size_t i, size, pos;
    uint32_t j;
    long version;

    version = strtol(proto->list[1], &end, 10);
    if (*proto->list[1] == '\0' || *end != '\0' || version < 1)
    {
        protocol_response_int(ERR_INVALID_OPERATION, proto);
        return;
    }

    size = 32;
    for (i = 0; i < EXTENSION_COUNT; i++)
    {
        want[i] = 0;
        for (j = 2; j < proto->list_count; j++)
            if (strcmp(proto->list[j], g_extensions[i]) == 0)
                want[i] = 1;
        size += strlen(g_extensions[i]) + 1;
    }

    buffer = MALLOC(size);
    if (!buffer)
    {
        protocol_response_int(ERR_MEMORY_ALLOCATION, proto);
        return;
    }
    memcpy(g_granted, want, sizeof(g_granted));
    pos = snprintf(buffer, size, "resp 0 %d", PLUGIN_HOSTD_PROTOCOL_VERSION);
    for (i = 0; i < EXTENSION_COUNT; i++)
        if (g_granted[i])
            pos += snprintf(buffer + pos, size - pos, " %s", g_extensions[i]);
    protocol_response(buffer, proto);
    FREE(buffer);
}

/* a character of a request tag: a letter, a digit, '-', '_' or '.' (grammar/core.abnf, tag-char) */
static int tag_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.';
}

/* sends an answer of size bytes, its NUL included; after "<tag> " when the request was tagged */
static void answer(const msg_t *msg, const char *tag, size_t tag_size, const char *text, size_t size)
{
    char *line = NULL;

    if (tag)
    {
        line = MALLOC(tag_size + 1 + size);
        /* with no memory for the tag the answer goes out bare, in its place: answers keep the order of requests */
        if (line)
        {
            memcpy(line, tag, tag_size);
            line[tag_size] = ' ';
            memcpy(line + tag_size + 1, text, size);
            text = line;
            size += tag_size + 1;
        }
    }
#ifndef SKIP_READLINE
    if (msg->sender_id == STDOUT_FILENO)
        write(msg->sender_id, text, size);
    else
#endif
        socket_send(msg->sender_id, text, size);
    if (line)
        FREE(line);
}


/*
************************************************************************************************************************
*           GLOBAL FUNCTIONS
************************************************************************************************************************
*/

void protocol_parse(msg_t *msg)
{
    uint32_t i, j;
    int32_t index;
    proto_t proto;
    char *data = msg->data, *tag = NULL;
    size_t tag_size = 0;

    // a session granted tags: a first word "@<tag>" is the request's tag, and the command follows it after one space
    if (data[0] == '@' && granted(OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS))
    {
        size_t n = 1;

        while (tag_char(data[n]))
            n++;
        if (n > 1 && (data[n] == ' ' || data[n] == '\0'))
        {
            tag = data;
            tag_size = n;
            data += data[n] == ' ' ? n + 1 : n;
        }
    }

    proto.list = strarr_split(data);
    proto.list_count = strarr_length(proto.list);
    proto.response = NULL;

    if (g_verbose)
    {
        printf("PROTOCOL: received '%s'", data);
        for (i = 1; i < proto.list_count; i++)
            printf(" '%s'", proto.list[i]);
        printf("\n");
    }

    // TODO: check invalid argumets (wildcards)

    // an empty line is not answered; an empty tagged one is answered, so every tagged request has its answer
    if (proto.list_count == 0)
    {
        if (tag)
        {
            const char *error = protocol_error_message(PROTOCOL_NOT_FOUND);
            answer(msg, tag, tag_size, error, strlen(error)+1);
        }
        FREE(proto.list);
        return;
    }

    unsigned int match, variable_arguments;
    int32_t refusal = PROTOCOL_NOT_FOUND, untyped = 0;
    int have_untyped = 0, have_refusal = 0;

    index = PROTOCOL_NOT_FOUND;

    // loop all registered commands: a verb may have more than one form, told apart by its number of arguments (the
    // plugin-hostd protocol's verb table), so every command of the verb is read, not only the first. The first form
    // the arguments fit, in number and in types, is run; else the first that fits in number, as mod-host reads a
    // wildcard; else the first form's refusal, which is mod-host's answer for a verb of one form
    for (i = 0; i < g_command_count; i++)
    {
        int32_t result;

        match = 0;
        variable_arguments = 0;

        // checks received protocol; the first word is the verb, so a command of another verb is not read further
        for (j = 0; j < proto.list_count && j < g_commands[i].count && (j == 0 || match > 0); j++)
        {
            if (strcmp(g_commands[i].list[j], proto.list[j]) == 0)
            {
                match++;
            }
            else if (match > 0)
            {
                if (is_wildcard(g_commands[i].list[j]))
                {
                    match++;
                }
                else if (strcmp(g_commands[i].list[j], "...") == 0)
                {
                    match++;
                    variable_arguments = 1;
                }
            }
        }

        if (match == 0)
            continue;

        // checks if the last argument is ...
        if (j < g_commands[i].count)
        {
            if (strcmp(g_commands[i].list[j], "...") == 0) variable_arguments = 1;
        }

        // few arguments
        if (proto.list_count < (g_commands[i].count - variable_arguments))
            result = PROTOCOL_FEW_ARGUMENTS;
        // many arguments
        else if (proto.list_count > g_commands[i].count && !variable_arguments)
            result = PROTOCOL_MANY_ARGUMENTS;
        // arguments match
        else if (match == proto.list_count || variable_arguments)
            result = (int32_t)i;
        // not found
        else
            result = PROTOCOL_NOT_FOUND;

        if (result >= 0 && typed(&g_commands[i], &proto))
        {
            index = result;
            break;
        }
        if (result >= 0 && !have_untyped)
        {
            untyped = result;
            have_untyped = 1;
        }
        else if (result < 0 && !have_refusal)
        {
            refusal = result;
            have_refusal = 1;
        }
    }
    if (index < 0)
        index = have_untyped ? untyped : refusal;

    // Protocol OK
    if (index >= 0)
    {
        if (g_commands[index].callback)
        {
            g_commands[index].callback(&proto);
            if (proto.response)
            {
                answer(msg, tag, tag_size, proto.response, proto.response_size+1);
                if (g_verbose) printf("PROTOCOL: response '%s'\n", proto.response);

                FREE(proto.response);
            }
        }
    }
    // Protocol error
    else
    {
        const char *error = protocol_error_message(index);
        answer(msg, tag, tag_size, error, strlen(error)+1);
        if (g_verbose) printf("PROTOCOL: error '%s'\n", error);
    }

    FREE(proto.list);
}


int protocol_add_command(const char *command, void (*callback)(proto_t *proto))
{
    if (g_command_count >= PROTOCOL_MAX_COMMANDS)
    {
        fprintf(stderr, "error: PROTOCOL_MAX_COMMANDS (%d) reached, '%s' is not registered\n", PROTOCOL_MAX_COMMANDS,
                command);
        return -1;
    }

    char *cmd = str_duplicate(command);
    g_commands[g_command_count].command = cmd;
    g_commands[g_command_count].list = strarr_split(cmd);
    g_commands[g_command_count].count = strarr_length(g_commands[g_command_count].list);
    g_commands[g_command_count].callback = callback;
    g_command_count++;
    return 0;
}


void protocol_response(const char *response, proto_t *proto)
{
    proto->response_size = strlen(response);
    proto->response = MALLOC(proto->response_size + 1);
    strcpy(proto->response, response);
}


void protocol_response_int(int resp, proto_t *proto)
{
    char buffer[32];
    snprintf(buffer, 32, "resp %i", resp);
    buffer[31] = '\0';
    protocol_response(buffer, proto);
}


int protocol_add_hello(void)
{
    socket_session_start = session_start;
    return protocol_add_command(OMX_VERB_HELLO_FMT, hello_cb);
}


void protocol_remove_commands(void)
{
    unsigned int i;

    session_start();

    for (i = 0; i < g_command_count; i++)
    {
        FREE(g_commands[i].command);
        FREE(g_commands[i].list);
    }
    g_command_count = 0;
}


const char *protocol_error_message(int code)
{
    if (code >= 0 || -code > (int)(sizeof(g_error_messages) / sizeof(g_error_messages[0])))
        return NULL;

    return g_error_messages[-code-1];
}

void protocol_verbose(int verbose)
{
    g_verbose = verbose;
}
