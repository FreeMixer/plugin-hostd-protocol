// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#ifndef PLUGIN_PIN_VOCABULARY_H
#define PLUGIN_PIN_VOCABULARY_H
/*
 * GENERATED: DO NOT EDIT BY HAND.
 * Produced by `omx-contract render --target c` from FreeMixer/plugin-pin's data/: the vocabulary
 * (docs/data-layout.md). A code is OMX_ and the code in upper snake case, and a reply code that carries a resp has it as that name and _RESP; a word is OMX_<ITEM>_<NAME>, each words
 * item with its _COUNT and an _INIT of its words in their order. A consumer writes and compares these, never a
 * literal of its own.
 */

/* PLUGIN_PIN_VERDICTS (data/pin.json) */
#define OMX_PLUGIN_PIN_VERDICTS_IN_PROCESS "in-process" ///< qualified to run in a worker that other plugins share
#define OMX_PLUGIN_PIN_VERDICTS_ISOLATED "isolated" ///< qualified to run, in a worker of its own
#define OMX_PLUGIN_PIN_VERDICTS_REFUSED "refused" ///< failed qualification: the pin records the bytes, a host should not load them
#define OMX_PLUGIN_PIN_VERDICTS_COUNT 3u
#define OMX_PLUGIN_PIN_VERDICTS_INIT { "in-process", "isolated", "refused" }

/* PLUGIN_PIN_POOLS (data/pin.json) */
#define OMX_PLUGIN_PIN_POOLS_CORES "cores" ///< its worker starts with the thread pool on, one helper per core of its cpu set but one, and no other plugin shares the worker
#define OMX_PLUGIN_PIN_POOLS_OFF "off" ///< measured, and not faster or not safe with the pool: the worker keeps no option
#define OMX_PLUGIN_PIN_POOLS_COUNT 2u
#define OMX_PLUGIN_PIN_POOLS_INIT { "cores", "off" }

/* PLUGIN_PIN_KEYS (data/pin.json) */
#define OMX_PLUGIN_PIN_KEYS_VERDICT "verdict" ///< the qualification verdict of the pinned bytes, a word of PLUGIN_PIN_VERDICTS
#define OMX_PLUGIN_PIN_KEYS_POOL "pool" ///< the thread pool word of the pinned bytes, a word of PLUGIN_PIN_POOLS
#define OMX_PLUGIN_PIN_KEYS_COUNT 2u
#define OMX_PLUGIN_PIN_KEYS_INIT { "verdict", "pool" }

/* PLUGIN_PIN_CHANGES (data/pin.json) */
#define OMX_PLUGIN_PIN_CHANGES_FILES "files" ///< a pinned file is gone, or its bytes no longer have the digest the pin holds
#define OMX_PLUGIN_PIN_CHANGES_LAYOUT "layout" ///< the plugin's parameter layout no longer has the fingerprint the pin holds
#define OMX_PLUGIN_PIN_CHANGES_COUNT 2u
#define OMX_PLUGIN_PIN_CHANGES_INIT { "files", "layout" }

/* PLUGIN_PIN_LAYOUT_FORMATS (data/pin.json) */
#define OMX_PLUGIN_PIN_LAYOUT_FORMATS_CLAP "clap" ///< every parameter clap.params gives, sorted by id
#define OMX_PLUGIN_PIN_LAYOUT_FORMATS_LV2 "lv2" ///< every control port, sorted by index
#define OMX_PLUGIN_PIN_LAYOUT_FORMATS_COUNT 2u
#define OMX_PLUGIN_PIN_LAYOUT_FORMATS_INIT { "clap", "lv2" }

#endif /* PLUGIN_PIN_VOCABULARY_H */
