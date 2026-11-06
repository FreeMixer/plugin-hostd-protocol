// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#ifndef PLUGIN_HOSTD_PROTOCOL_VOCABULARY_H
#define PLUGIN_HOSTD_PROTOCOL_VOCABULARY_H
/*
 * GENERATED: DO NOT EDIT BY HAND.
 * Produced by `omx-contract render --target c` from FreeMixer/plugin-hostd-protocol's data/: the vocabulary
 * (docs/data-layout.md). A code is OMX_ and the code in upper snake case, and a reply code that carries a resp has it as that name and _RESP; a word is OMX_<ITEM>_<NAME>, each words
 * item with its _COUNT and an _INIT of its words in their order. A consumer writes and compares these, never a
 * literal of its own.
 */

/* PLUGIN_HOSTD_REPLY_CODES (data/verbs.json) */
#define OMX_REPLY_PLUGIN_HOSTD_PLACEMENT_INVALID "reply.plugin-hostd.placement-invalid" ///< not a placement, or not valid here
#define OMX_REPLY_PLUGIN_HOSTD_PLACEMENT_INVALID_RESP (-501)
#define OMX_REPLY_PLUGIN_HOSTD_NO_BACKEND "reply.plugin-hostd.no-backend" ///< no worker program for the scheme
#define OMX_REPLY_PLUGIN_HOSTD_NO_BACKEND_RESP (-502)
#define OMX_REPLY_PLUGIN_HOSTD_WORKER_SPAWN "reply.plugin-hostd.worker-spawn" ///< the worker is not up
#define OMX_REPLY_PLUGIN_HOSTD_WORKER_SPAWN_RESP (-503)
#define OMX_REPLY_PLUGIN_HOSTD_REPLAY "reply.plugin-hostd.replay" ///< the name of a refused replay on stderr, never a reply
#define OMX_REPLY_PLUGIN_HOSTD_REPLAY_RESP (-504)
#define OMX_REPLY_PLUGIN_HOSTD_GAVE_UP "reply.plugin-hostd.gave-up" ///< the storm bound is spent for that placement
#define OMX_REPLY_PLUGIN_HOSTD_GAVE_UP_RESP (-505)
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_WORKER "reply.plugin-hostd.no-such-worker" ///< no such worker
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_WORKER_RESP (-506)
#define OMX_REPLY_PLUGIN_HOSTD_VERB_DROPPED "reply.plugin-hostd.verb-dropped" ///< the worker died on this very command and the daemon dropped it
#define OMX_REPLY_PLUGIN_HOSTD_VERB_DROPPED_RESP (-507)
#define OMX_REPLY_PLUGIN_HOSTD_PIN_BINARY_MISMATCH "reply.plugin-hostd.pin-binary-mismatch" ///< a pinned file is missing or its SHA-256 differs
#define OMX_REPLY_PLUGIN_HOSTD_PIN_BINARY_MISMATCH_RESP (-509)
#define OMX_REPLY_PLUGIN_HOSTD_LV2_ADAPTER_REFUSED "reply.plugin-hostd.lv2-adapter-refused" ///< the lv2-clap worker's LV2 adapter cannot run this plugin
#define OMX_REPLY_PLUGIN_HOSTD_LV2_ADAPTER_REFUSED_RESP (-512)
#define OMX_REPLY_PLUGIN_HOSTD_NO_PRESET_LIST "reply.plugin-hostd.no-preset-list" ///< the instance's worker cannot list the plugin's presets
#define OMX_REPLY_PLUGIN_HOSTD_NO_PRESET_LIST_RESP (-515)
#define OMX_REPLY_PLUGIN_HOSTD_PIN_REFUSED "reply.plugin-hostd.pin-refused" ///< the plugin's pin carries verdict=refused
#define OMX_REPLY_PLUGIN_HOSTD_PIN_REFUSED_RESP (-516)
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_UNKNOWN_INSTANCE "reply.plugin-hostd.chain-unknown-instance" ///< a member is an instance no add made
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_UNKNOWN_INSTANCE_RESP (-518)
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_DUPLICATE "reply.plugin-hostd.chain-duplicate" ///< an instance is named twice in one chain
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_DUPLICATE_RESP (-519)
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_INSTANCE_BUSY "reply.plugin-hostd.chain-instance-busy" ///< a member is already in another chain
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_INSTANCE_BUSY_RESP (-520)
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_MIXED_FORMAT "reply.plugin-hostd.chain-mixed-format" ///< no one worker program runs every member
#define OMX_REPLY_PLUGIN_HOSTD_CHAIN_MIXED_FORMAT_RESP (-521)
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_CHAIN "reply.plugin-hostd.no-such-chain" ///< a chain no chain line declared
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_CHAIN_RESP (-522)
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_NOT_OFFERED "reply.plugin-hostd.preset-not-offered" ///< a stable name the plugin does not offer
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_NOT_OFFERED_RESP (-523)
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_INVALID "reply.plugin-hostd.preset-invalid" ///< a preset file or path that is not the preset format
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_INVALID_RESP (-524)
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_NEEDS_STATE "reply.plugin-hostd.preset-needs-state" ///< a preset file that carries state for a worker that cannot take state
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_NEEDS_STATE_RESP (-525)
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_PLUGIN_CHANGED "reply.plugin-hostd.preset-plugin-changed" ///< the preset file's plugin id, version or fingerprint is not the instance's
#define OMX_REPLY_PLUGIN_HOSTD_PRESET_PLUGIN_CHANGED_RESP (-526)
#define OMX_REPLY_PLUGIN_HOSTD_LOAD_REFUSED "reply.plugin-hostd.load-refused" ///< the plugin refused the preset or the state its file carries
#define OMX_REPLY_PLUGIN_HOSTD_LOAD_REFUSED_RESP (-527)
#define OMX_REPLY_PLUGIN_HOSTD_DIR_NOT_WRITABLE "reply.plugin-hostd.dir-not-writable" ///< descriptor_dir or qualification_dir cannot be made or written
#define OMX_REPLY_PLUGIN_HOSTD_DIR_NOT_WRITABLE_RESP (-528)
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_ABSENT "reply.plugin-hostd.descriptor-absent" ///< no descriptor (panel) for the plugin's binary as it is now
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_ABSENT_RESP (-529)
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_PENDING "reply.plugin-hostd.descriptor-pending" ///< the scan its add started has not written the descriptor (panel) yet
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_PENDING_RESP (-530)
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_SCAN_FAILED "reply.plugin-hostd.descriptor-scan-failed" ///< the plugin's last scan wrote no descriptor (panel)
#define OMX_REPLY_PLUGIN_HOSTD_DESCRIPTOR_SCAN_FAILED_RESP (-531)
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_PLAN "reply.plugin-hostd.no-such-plan" ///< a chain no plan verb planned
#define OMX_REPLY_PLUGIN_HOSTD_NO_SUCH_PLAN_RESP (-532)
#define OMX_REPLY_PLUGIN_HOSTD_NO_RECORD "reply.plugin-hostd.no-record" ///< no qualification record names the plugin
#define OMX_REPLY_PLUGIN_HOSTD_NO_RECORD_RESP (-533)
#define OMX_REPLY_PLUGIN_HOSTD_NO_CELL "reply.plugin-hostd.no-cell" ///< the plugin's qualification records hold no cost and latency at that rate and quantum
#define OMX_REPLY_PLUGIN_HOSTD_NO_CELL_RESP (-534)
#define OMX_REPLY_PLUGIN_HOSTD_UNKNOWN_PLUGIN "reply.plugin-hostd.unknown-plugin" ///< the uri names no plugin installed here
#define OMX_REPLY_PLUGIN_HOSTD_UNKNOWN_PLUGIN_RESP (-535)
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_KIND_UNKNOWN "reply.plugin-hostd.curve-kind-unknown" ///< curve_bind of a family with no closed form for the display's kind, or curve_points of a binding whose values pick a band shape or a head filter slope with no closed form
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_KIND_UNKNOWN_RESP (-536)
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_TERM_MISSING "reply.plugin-hostd.curve-term-missing" ///< curve_points of a display with no binding, or of a binding that lacks a term the model needs
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_TERM_MISSING_RESP (-537)
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_UNIT_REFUSED "reply.plugin-hostd.curve-unit-refused" ///< curve_bind of a term in a unit its role does not take
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_UNIT_REFUSED_RESP (-538)
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_POINTS_RANGE "reply.plugin-hostd.curve-points-range" ///< curve_points of fewer than 2 points or more than the most a curve carries, an EQ's three anchors a drawn band counted, or of a band index past what fits
#define OMX_REPLY_PLUGIN_HOSTD_CURVE_POINTS_RANGE_RESP (-539)
#define OMX_REPLY_PLUGIN_HOSTD_DURABLE_WRITE_FAILED "reply.plugin-hostd.durable-write-failed" ///< the daemon's write of a file the verb changes (the history, a pin file) failed on its writer thread: the change is not on disk
#define OMX_REPLY_PLUGIN_HOSTD_DURABLE_WRITE_FAILED_RESP (-540)
#define OMX_REPLY_PLUGIN_HOSTD_WRITER_QUEUE_FULL "reply.plugin-hostd.writer-queue-full" ///< the daemon's writer queue holds its most writes (64) or bytes (64 MiB): the write the verb needs was refused, and nothing of it changed
#define OMX_REPLY_PLUGIN_HOSTD_WRITER_QUEUE_FULL_RESP (-541)

/* PLUGIN_HOSTD_PROTOCOL_EXTENSIONS (data/verbs.json) */
#define OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_TAGS "tags" ///< request tags: a line may start with @<tag>, and its answer then starts with the same @<tag>
#define OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_ERRORS "errors" ///< error words: a refusal resp <n> is followed by the stable word of its code, reply.<group>.<reason>, when the protocol declares one
#define OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_SEQ "seq" ///< event sequence numbers: every feedback line starts with #<n>, 1 for the first after this hello and one more for each, so a gap is a missed line
#define OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_COUNT 3u
#define OMX_PLUGIN_HOSTD_PROTOCOL_EXTENSIONS_INIT { "tags", "errors", "seq" }

#endif /* PLUGIN_HOSTD_PROTOCOL_VOCABULARY_H */
