/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* Between the socket and the parser, inside the library: socket_run() calls socket_session_start each time a controller
 * connects, so what an earlier connection's hello granted never reaches the next one. protocol_add_hello() sets it. Not
 * exported, and not a header a consumer sees. */

#ifndef SESSION_H
#define SESSION_H

#include <stddef.h>

extern void (*socket_session_start)(void) __attribute__((visibility("hidden")));

/* socket_send_feedback() asks it for the words a feedback line starts with: "#<n> " in a session granted seq, the line's
 * number, or nothing. It writes them, NUL-terminated, in buf and answers their length. protocol_add_hello() sets it. */
extern size_t (*socket_feedback_prefix)(char *buf, size_t size) __attribute__((visibility("hidden")));

#endif
