/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com> */

/* Between the socket and the parser, inside the library: socket_run() calls socket_session_start each time a controller
 * connects, so what an earlier connection's hello granted never reaches the next one. protocol_add_hello() sets it. Not
 * exported, and not a header a consumer sees. */

#ifndef SESSION_H
#define SESSION_H

extern void (*socket_session_start)(void) __attribute__((visibility("hidden")));

#endif
