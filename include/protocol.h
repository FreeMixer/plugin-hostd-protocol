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
*
************************************************************************************************************************
*/

#ifndef PROTOCOL_H
#define PROTOCOL_H


/*
************************************************************************************************************************
*           INCLUDE FILES
************************************************************************************************************************
*/

#include "utils.h"
#include "socket.h"

/*
************************************************************************************************************************
*           DO NOT CHANGE THESE DEFINES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           CONFIGURATION DEFINES
************************************************************************************************************************
*/

#define PROTOCOL_MAX_COMMANDS       128

// error messages configuration
#define MESSAGE_COMMAND_NOT_FOUND   "not found"
#define MESSAGE_MANY_ARGUMENTS      "many arguments"
#define MESSAGE_FEW_ARGUMENTS       "few arguments"
#define MESSAGE_INVALID_ARGUMENT    "invalid argument"

// protocol_parse() errors, answered with protocol_error_message()
#define PROTOCOL_NOT_FOUND          (-1)
#define PROTOCOL_MANY_ARGUMENTS     (-2)
#define PROTOCOL_FEW_ARGUMENTS      (-3)
#define PROTOCOL_INVALID_ARGUMENT   (-4)

// this library answers hello and request tags (doc/protocol.md, Sessions) once a host calls protocol_add_hello(); a
// consumer that also builds against mod-host's library tests this before it calls it
#define PROTOCOL_HELLO              1


/*
************************************************************************************************************************
*           DATA TYPES
************************************************************************************************************************
*/

// This struct is used on callbacks argument
typedef struct PROTO_T {
    char **list;
    uint32_t list_count;
    char *response;
    uint32_t response_size;
} proto_t;


/*
************************************************************************************************************************
*           GLOBAL VARIABLES
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           MACRO'S
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           FUNCTION PROTOTYPES
************************************************************************************************************************
*/

void protocol_parse(msg_t *msg);
/* 0, or -1 when the table already holds PROTOCOL_MAX_COMMANDS commands: the command is not registered, and stderr
 * says which */
int protocol_add_command(const char *command, void (*callback)(proto_t *proto));
void protocol_response(const char *response, proto_t *proto);
void protocol_response_int(int resp, proto_t *proto);
void protocol_remove_commands(void);
/* Registers hello, which the library answers itself: the client names its protocol version and the extensions it asks
 * for, and the reply names the protocol version and the extensions granted, from the declared table
 * (doc/protocol.md, Sessions). A session granted tags may tag a line, and its answer carries the tag. What hello
 * grants lasts until the next hello or the next connection; a client that never sends hello is answered as before.
 * 0, or -1 as protocol_add_command */
int protocol_add_hello(void);
void protocol_verbose(int verbose);
const char *protocol_error_message(int code);


/*
************************************************************************************************************************
*           CONFIGURATION ERRORS
************************************************************************************************************************
*/


/*
************************************************************************************************************************
*           END HEADER
************************************************************************************************************************
*/

#endif
