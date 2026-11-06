/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol: the curve verbs' state, inside the library (src/host-curve.c).
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

#ifndef HOST_CURVE_INTERNAL_H
#define HOST_CURVE_INTERNAL_H

#include "host-backend.h"

/* the instance remove names: its bindings go with it; -1 (mod-host's remove of every instance) drops them all */
void host_curve_forget(int instance) __attribute__((visibility("hidden")));
/* the backend whose param_get the curve verbs read: host_dispatch_register()'s */
void host_curve_set_backend(const host_backend_t *backend) __attribute__((visibility("hidden")));
/* the reply code of an omx_curve_points code */
int host_curve_code(int omx_code) __attribute__((visibility("hidden")));

#endif
