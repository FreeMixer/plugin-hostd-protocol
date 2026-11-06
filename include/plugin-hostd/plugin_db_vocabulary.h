// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#ifndef PLUGIN_DB_VOCABULARY_H
#define PLUGIN_DB_VOCABULARY_H
/*
 * GENERATED: DO NOT EDIT BY HAND.
 * Produced by `omx-contract render --target c` from FreeMixer/plugin-db's data/: the vocabulary
 * (docs/data-layout.md). A code is OMX_ and the code in upper snake case, and a reply code that carries a resp has it as that name and _RESP; a word is OMX_<ITEM>_<NAME>, each words
 * item with its _COUNT and an _INIT of its words in their order. A consumer writes and compares these, never a
 * literal of its own.
 */

/* PLUGIN_DB_ADDRESS_FORMATS (data/db.json) */
#define OMX_PLUGIN_DB_ADDRESS_FORMATS_CLAP "clap" ///< a CLAP plugin: the id is the plugin descriptor's id
#define OMX_PLUGIN_DB_ADDRESS_FORMATS_LV2 "lv2" ///< an LV2 plugin: the id is the plugin's URI
#define OMX_PLUGIN_DB_ADDRESS_FORMATS_COUNT 2u
#define OMX_PLUGIN_DB_ADDRESS_FORMATS_INIT { "clap", "lv2" }

/* PLUGIN_DB_BUNDLE_RULES (data/db.json) */
#define OMX_PLUGIN_DB_BUNDLE_RULES_NEWEST_VERSION "newest-version" ///< of the candidates, the one whose manifest gives the plugin the highest lv2:minorVersion, then the highest lv2:microVersion, a version not stated counting as 0; of equal versions the first in the search path, its directories in order and the bundles of one directory in byte order of their names. This is lilv's rule, so the host loads the bundle plugin-db pins.
#define OMX_PLUGIN_DB_BUNDLE_RULES_COUNT 1u
#define OMX_PLUGIN_DB_BUNDLE_RULES_INIT { "newest-version" }

/* PLUGIN_DB_LAYERS (data/db.json) */
#define OMX_PLUGIN_DB_LAYERS_DECLARED "declared" ///< meaning, units, travel and defaults, installed with the package by the plugin's owner, read only
#define OMX_PLUGIN_DB_LAYERS_SCANNED "scanned" ///< identity, controls, ports, presets and GUI evidence, written by plugin-qualify under its runner
#define OMX_PLUGIN_DB_LAYERS_PINNED "pinned" ///< the files of the plugin with their digests and the layout fingerprint, written through plugin-db
#define OMX_PLUGIN_DB_LAYERS_MEASURED "measured" ///< latency, cost and behaviour per rate and quantum with the conditions, written by the probes; may be wiped
#define OMX_PLUGIN_DB_LAYERS_JUDGED "judged" ///< the record: verdict, conditions and what it was measured under, written by plugin-qualify's judgement
#define OMX_PLUGIN_DB_LAYERS_REFERENCE "reference" ///< a record from a reference machine, installed by plugin-db-qualification, read only
#define OMX_PLUGIN_DB_LAYERS_CORRECTED "corrected" ///< renames, hidden controls and scale a person chose, written by the override module
#define OMX_PLUGIN_DB_LAYERS_WARNED_OVERRIDE "warned-override" ///< a person's choice to run a refused plugin anyway, alone; it binds the pin it was written for and does not survive an upgrade
#define OMX_PLUGIN_DB_LAYERS_OBSERVED "observed" ///< crash and xrun history, written by plugin-hostd and the engine
#define OMX_PLUGIN_DB_LAYERS_COUNT 9u
#define OMX_PLUGIN_DB_LAYERS_INIT { "declared", "scanned", "pinned", "measured", "judged", "reference", "corrected", "warned-override", "observed" }

/* PLUGIN_DB_FILES (data/db.json) */
#define OMX_PLUGIN_DB_FILES_SAME "same" ///< every file the plugin loads is held by the pin and has the digest the pin holds
#define OMX_PLUGIN_DB_FILES_CHANGED "changed" ///< a file differs from its digest, is gone, is new, or another bundle is now the plugin
#define OMX_PLUGIN_DB_FILES_ABSENT "absent" ///< the plugin is in no file of the search path any more, or its pinned file is gone
#define OMX_PLUGIN_DB_FILES_COULD_NOT_TEST "could-not-test" ///< there is no pinned layer, or a file or the pin could not be read
#define OMX_PLUGIN_DB_FILES_COUNT 4u
#define OMX_PLUGIN_DB_FILES_INIT { "same", "changed", "absent", "could-not-test" }

/* PLUGIN_DB_LAYOUT (data/db.json) */
#define OMX_PLUGIN_DB_LAYOUT_SAME "same" ///< the layout has the pinned fingerprint: the files are the pinned bytes, or a host found the pinned fingerprint
#define OMX_PLUGIN_DB_LAYOUT_CHANGED "changed" ///< a host found another fingerprint than the pinned one
#define OMX_PLUGIN_DB_LAYOUT_NOT_YET_KNOWN "not-yet-known" ///< the pin holds no layout, or the files changed and no host has found their layout yet
#define OMX_PLUGIN_DB_LAYOUT_COUNT 3u
#define OMX_PLUGIN_DB_LAYOUT_INIT { "same", "changed", "not-yet-known" }

/* PLUGIN_DB_RECORD (data/db.json) */
#define OMX_PLUGIN_DB_RECORD_CURRENT "current" ///< it was written for the pinned bytes and layout, and nothing of PLUGIN_DB_RECORD_CHANGES makes it stale
#define OMX_PLUGIN_DB_RECORD_STALE "stale" ///< it exists, and a change of PLUGIN_DB_RECORD_CHANGES, or files that could not be tested, keep it from answering
#define OMX_PLUGIN_DB_RECORD_NONE "none" ///< there is no judged layer
#define OMX_PLUGIN_DB_RECORD_COUNT 3u
#define OMX_PLUGIN_DB_RECORD_INIT { "current", "stale", "none" }

/* PLUGIN_DB_RECORD_ACTIONS (data/db.json) */
#define OMX_PLUGIN_DB_RECORD_ACTIONS_FULL_PATH "full-path" ///< scan, pin, measure and judge again
#define OMX_PLUGIN_DB_RECORD_ACTIONS_MEASURE_HERE "measure-here" ///< measure and judge on this class of machine; the record stays valid for its own class
#define OMX_PLUGIN_DB_RECORD_ACTIONS_CONVERT "convert" ///< convert the record to the schema in force, with no new measurement
#define OMX_PLUGIN_DB_RECORD_ACTIONS_REJUDGE "rejudge" ///< judge the measurements again, automatically, with no new measurement
#define OMX_PLUGIN_DB_RECORD_ACTIONS_REMEASURE_BACKGROUND "remeasure-background" ///< the record stays current, shown as measured under the host it names, and is measured again in the background
#define OMX_PLUGIN_DB_RECORD_ACTIONS_COUNT 5u
#define OMX_PLUGIN_DB_RECORD_ACTIONS_INIT { "full-path", "measure-here", "convert", "rejudge", "remeasure-background" }

/* PLUGIN_DB_RECORD_CHANGES (data/db.json) */
#define OMX_PLUGIN_DB_RECORD_CHANGES_FILES "files" ///< the plugin's files are not the bytes the record was written for, or could not be tested
#define OMX_PLUGIN_DB_RECORD_CHANGES_LAYOUT "layout" ///< the parameter layout is not the one the record was written for, or is not yet known
#define OMX_PLUGIN_DB_RECORD_CHANGES_HOSTING_CHANGE "hosting-change" ///< the host is a release that declares a hosting change: its hosting epoch is past the record's. The host's owner declares it in the contract; no reader guesses
#define OMX_PLUGIN_DB_RECORD_CHANGES_MACHINE_CLASS "machine-class" ///< this machine is of another class than the record's
#define OMX_PLUGIN_DB_RECORD_CHANGES_SCHEMA "schema" ///< the record schema in force is not the record's
#define OMX_PLUGIN_DB_RECORD_CHANGES_RULES "rules" ///< the judgement rules in force are not the ones the record was judged by
#define OMX_PLUGIN_DB_RECORD_CHANGES_HOST_VERSION "host-version" ///< the host is a normal release other than the one the record was measured under
#define OMX_PLUGIN_DB_RECORD_CHANGES_COUNT 7u
#define OMX_PLUGIN_DB_RECORD_CHANGES_INIT { "files", "layout", "hosting-change", "machine-class", "schema", "rules", "host-version" }

/* PLUGIN_DB_STATES (data/db.json) */
#define OMX_PLUGIN_DB_STATES_UNQUALIFIED "unqualified" ///< no current record, for any reason: runs alone in its own limited worker, never grouped; shown as not qualified, with the reason
#define OMX_PLUGIN_DB_STATES_QUALIFIED "qualified" ///< a current record whose verdict allows it and whose conditions are met here: runs as the verdict and the placement rules allow
#define OMX_PLUGIN_DB_STATES_ISOLATED_ONLY "isolated-only" ///< a current record that says it is safe only alone: runs alone
#define OMX_PLUGIN_DB_STATES_CONDITIONS_UNMET "conditions-unmet" ///< a current record with no cell for this rate or quantum, or a condition that fails here: runs alone, as unqualified
#define OMX_PLUGIN_DB_STATES_REFUSED "refused" ///< a current record or a shipped refusal says refused: not loaded, unless a warned override says run alone
#define OMX_PLUGIN_DB_STATES_ABSENT "absent" ///< the files are gone: not loaded
#define OMX_PLUGIN_DB_STATES_COUNT 6u
#define OMX_PLUGIN_DB_STATES_INIT { "unqualified", "qualified", "isolated-only", "conditions-unmet", "refused", "absent" }

/* PLUGIN_DB_RUNS (data/db.json) */
#define OMX_PLUGIN_DB_RUNS_SHARED "shared" ///< as the verdict and the placement rules allow, in a worker other plugins may share
#define OMX_PLUGIN_DB_RUNS_ALONE "alone" ///< in its own limited worker, never grouped
#define OMX_PLUGIN_DB_RUNS_NOT_LOADED "not-loaded" ///< not loaded: shown with its reason and its saved values kept; an effect passes audio through, an instrument is silent
#define OMX_PLUGIN_DB_RUNS_COUNT 3u
#define OMX_PLUGIN_DB_RUNS_INIT { "shared", "alone", "not-loaded" }

/* PLUGIN_DB_REASONS (data/db.json) */
#define OMX_PLUGIN_DB_REASONS_NEVER_PINNED "never-pinned" ///< there is no pinned layer: never scanned or never pinned
#define OMX_PLUGIN_DB_REASONS_MEASUREMENT_PENDING "measurement-pending" ///< pinned, and there is no record yet
#define OMX_PLUGIN_DB_REASONS_COULD_NOT_TEST "could-not-test" ///< the files or the pin could not be read
#define OMX_PLUGIN_DB_REASONS_FILES_CHANGED "files-changed" ///< the files are not the pinned bytes
#define OMX_PLUGIN_DB_REASONS_LAYOUT_CHANGED "layout-changed" ///< the layout is not the pinned one
#define OMX_PLUGIN_DB_REASONS_LAYOUT_NOT_YET_KNOWN "layout-not-yet-known" ///< the pin holds no layout, so a record cannot bind one
#define OMX_PLUGIN_DB_REASONS_RECORD_STALE "record-stale" ///< the record is stale for a change of PLUGIN_DB_RECORD_CHANGES, which show names with its action
#define OMX_PLUGIN_DB_REASONS_NOT_MEASURED_UNDER_HOST "not-measured-under-host" ///< the verdict says alone, and the record was not measured under plugin-hostd, which isolated only needs
#define OMX_PLUGIN_DB_REASONS_VERDICT_ALLOWS "verdict-allows" ///< the record's verdict allows a shared worker and its conditions are met here
#define OMX_PLUGIN_DB_REASONS_VERDICT_ISOLATED "verdict-isolated" ///< the record's verdict says it is safe only alone
#define OMX_PLUGIN_DB_REASONS_REFERENCE_ONLY "reference-only" ///< a shipped record matches the pin and was measured on another class of machine: reference figures only, until measured here
#define OMX_PLUGIN_DB_REASONS_NO_CELL "no-cell" ///< the record has no cell for this rate and quantum
#define OMX_PLUGIN_DB_REASONS_CONDITION_FAILS "condition-fails" ///< a condition of the record does not hold here, or is not known here
#define OMX_PLUGIN_DB_REASONS_RECORD_REFUSED "record-refused" ///< the record says refused; the way back is a rescan, a requalification or a warned override that runs it alone
#define OMX_PLUGIN_DB_REASONS_SHIPPED_REFUSAL "shipped-refusal" ///< a shipped record of these bytes says refused, which wins over everything and no warned override overrides
#define OMX_PLUGIN_DB_REASONS_FILES_GONE "files-gone" ///< the plugin's files are gone; show names the path that was pinned
#define OMX_PLUGIN_DB_REASONS_COUNT 16u
#define OMX_PLUGIN_DB_REASONS_INIT { "never-pinned", "measurement-pending", "could-not-test", "files-changed", "layout-changed", "layout-not-yet-known", "record-stale", "not-measured-under-host", "verdict-allows", "verdict-isolated", "reference-only", "no-cell", "condition-fails", "record-refused", "shipped-refusal", "files-gone" }

/* PLUGIN_DB_ISOLATION_HOSTS (data/db.json) */
#define OMX_PLUGIN_DB_ISOLATION_HOSTS_PLUGIN_HOSTD "plugin-hostd" ///< the daemon that hosts plugins for use, in workers of their own
#define OMX_PLUGIN_DB_ISOLATION_HOSTS_COUNT 1u
#define OMX_PLUGIN_DB_ISOLATION_HOSTS_INIT { "plugin-hostd" }

/* PLUGIN_DB_SOURCES (data/db.json) */
#define OMX_PLUGIN_DB_SOURCES_NONE "none" ///< no record answers
#define OMX_PLUGIN_DB_SOURCES_LOCAL "local" ///< the judged layer: measured here
#define OMX_PLUGIN_DB_SOURCES_REFERENCE "reference" ///< the reference layer: a shipped record, measured on a reference machine
#define OMX_PLUGIN_DB_SOURCES_COUNT 3u
#define OMX_PLUGIN_DB_SOURCES_INIT { "none", "local", "reference" }

/* PLUGIN_DB_PIN_VERDICTS (data/pin.json) */
#define OMX_PLUGIN_DB_PIN_VERDICTS_IN_PROCESS "in-process" ///< qualified to run in a worker that other plugins share
#define OMX_PLUGIN_DB_PIN_VERDICTS_ISOLATED "isolated" ///< qualified to run, in a worker of its own
#define OMX_PLUGIN_DB_PIN_VERDICTS_REFUSED "refused" ///< failed qualification: the pin records the bytes, a host should not load them
#define OMX_PLUGIN_DB_PIN_VERDICTS_COUNT 3u
#define OMX_PLUGIN_DB_PIN_VERDICTS_INIT { "in-process", "isolated", "refused" }

/* PLUGIN_DB_PIN_POOLS (data/pin.json) */
#define OMX_PLUGIN_DB_PIN_POOLS_CORES "cores" ///< its worker starts with the thread pool on, one helper per core of its cpu set but one, and no other plugin shares the worker
#define OMX_PLUGIN_DB_PIN_POOLS_OFF "off" ///< measured, and not faster or not safe with the pool: the worker keeps no option
#define OMX_PLUGIN_DB_PIN_POOLS_COUNT 2u
#define OMX_PLUGIN_DB_PIN_POOLS_INIT { "cores", "off" }

/* PLUGIN_DB_PIN_KEYS (data/pin.json) */
#define OMX_PLUGIN_DB_PIN_KEYS_VERDICT "verdict" ///< the qualification verdict of the pinned bytes, a word of PLUGIN_DB_PIN_VERDICTS
#define OMX_PLUGIN_DB_PIN_KEYS_POOL "pool" ///< the thread pool word of the pinned bytes, a word of PLUGIN_DB_PIN_POOLS
#define OMX_PLUGIN_DB_PIN_KEYS_COUNT 2u
#define OMX_PLUGIN_DB_PIN_KEYS_INIT { "verdict", "pool" }

/* PLUGIN_DB_PIN_CHANGES (data/pin.json) */
#define OMX_PLUGIN_DB_PIN_CHANGES_FILES "files" ///< a pinned file is gone, or its bytes no longer have the digest the pin holds
#define OMX_PLUGIN_DB_PIN_CHANGES_LAYOUT "layout" ///< the plugin's parameter layout no longer has the fingerprint the pin holds
#define OMX_PLUGIN_DB_PIN_CHANGES_COUNT 2u
#define OMX_PLUGIN_DB_PIN_CHANGES_INIT { "files", "layout" }

/* PLUGIN_DB_PIN_LAYOUT_FORMATS (data/pin.json) */
#define OMX_PLUGIN_DB_PIN_LAYOUT_FORMATS_CLAP "clap" ///< every parameter clap.params gives, sorted by id
#define OMX_PLUGIN_DB_PIN_LAYOUT_FORMATS_LV2 "lv2" ///< every control port, sorted by index
#define OMX_PLUGIN_DB_PIN_LAYOUT_FORMATS_COUNT 2u
#define OMX_PLUGIN_DB_PIN_LAYOUT_FORMATS_INIT { "clap", "lv2" }

#endif /* PLUGIN_DB_VOCABULARY_H */
