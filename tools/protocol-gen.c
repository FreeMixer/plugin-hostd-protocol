/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * This file is part of plugin-hostd-protocol.
 *
 * plugin-hostd-protocol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * plugin-hostd-protocol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with plugin-hostd-protocol.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
 */

/* What this library generates from include/plugin-hostd/protocol.h, compiled against it, so the values are the
 * header's own (a consumer generates its own pages from the installed header the same way):
 *
 *   protocol-gen json          protocol/plugin-hostd.json on stdout
 *   protocol-gen regions       the name of every region, one per line
 *   protocol-gen splice FILE   FILE on stdout with every generated region replaced; a region is the lines between
 *                              "BEGIN GENERATED protocol:<name>" and "END GENERATED protocol:<name>" */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <omxcontract/omx_contract_limits.h>
#include <omxcontract/omx_vocabulary.h>
#include <plugin-hostd/protocol.h>

/* The parameter scales are omx-contract's PARAM_SCALES, whose words the contract declares for this protocol: the same
 * ids in the same order, or nothing generated from the header builds. The contract renders an id as its word in
 * capitals, which scales_are_the_contracts() checks of every word at run time. */
enum {
#define X(id, name, meaning) PHD_SCALE_AT_##id,
    PHD_PARAM_SCALES(X)
#undef X
    PHD_SCALE_COUNT
};
#define X(id, name, meaning) \
    _Static_assert((int)PHD_SCALE_AT_##id == (int)OMX_PARAM_SCALES_##id, "PHD_PARAM_SCALES: " #id " is not where omx-contract's PARAM_SCALES has it");
PHD_PARAM_SCALES(X)
#undef X
_Static_assert((unsigned)PHD_SCALE_COUNT == OMX_PARAM_SCALES_COUNT, "PHD_PARAM_SCALES: not as many scales as omx-contract's PARAM_SCALES");

static int scales_are_the_contracts(void)
{
    int ok = 1;
    size_t n;

#define X(id, name, meaning) \
    for (n = 0; n <= strlen(name); n++) \
        if (toupper((unsigned char)name[n]) != #id[n]) \
        { \
            fprintf(stderr, "protocol-gen: PHD_PARAM_SCALES: the word \"%s\" is not omx-contract's PARAM_SCALES id " #id "\n", name); \
            ok = 0; \
            break; \
        }
    PHD_PARAM_SCALES(X)
#undef X
    return ok;
}

/* ---------------------------------------------------------------- json */

static void jstr(const char *s)
{
    putchar('"');
    for (; *s; s++)
    {
        if (*s == '"' || *s == '\\')
            putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

static void json(void)
{
    const char *sep;

    printf("{\n  \"protocol\": \"plugin-hostd\",\n  \"version\": %d,\n", PLUGIN_HOSTD_PROTOCOL_VERSION);
    printf("  \"readiness\": {\"daemon_line\": ");
    jstr(PHD_READY_LINE);
    printf(", \"worker_marker\": ");
    jstr(PHD_WORKER_READY_MARKER);
    printf("},\n  \"constants\": [");
    sep = "\n";
#define X(name, value, unit, meaning) \
    printf("%s    {\"name\": \"" #name "\", \"value\": %d, \"unit\": \"" unit "\", \"meaning\": ", sep, value); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_CONSTANTS(X)
#undef X
    printf("\n  ],\n  \"placement\": {\n    \"own\": ");
    jstr(PHD_PLACE_OWN);
    printf(",\n    \"default\": ");
    jstr(PHD_PLACE_DEFAULT);
    printf(",\n    \"pool_prefix\": ");
    jstr(PHD_PLACE_POOL_PREFIX);
    printf(",\n    \"pool_sibling_separator\": ");
    jstr(PHD_POOL_SIBLING_SEPARATOR);
    printf(",\n    \"syntax\": ");
    jstr(PHD_PLACEMENT_SYNTAX);
    printf(",\n    \"pool_name\": {\"pattern\": ");
    jstr(PHD_POOL_NAME_PATTERN);
    printf(", \"class\": ");
    jstr(PHD_POOL_NAME_CLASS);
    printf(", \"min\": %d, \"max\": %d}\n  },\n", PHD_POOL_NAME_MIN, PHD_POOL_NAME_MAX);
    printf("  \"formats\": [");
    sep = "";
#define X(id, name) printf("%s\"%s\"", sep, name); sep = ", ";
    PHD_FORMATS(X)
#undef X
    printf("],\n  \"words\": {\"all\": ");
    jstr(PHD_WORD_ALL);
    printf(", \"none\": ");
    jstr(PHD_WORD_NONE);
    printf(", \"any_format\": ");
    jstr(PHD_FORMAT_ANY);
    printf("},\n  \"worker_states\": [");
    sep = "\n";
#define X(id, name, meaning) \
    printf("%s    {\"name\": \"%s\", \"meaning\": ", sep, name); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_WORKER_STATES(X)
#undef X
    printf("\n  ],\n  \"errors\": [");
    sep = "\n";
#define X(id, resp, meaning) \
    printf("%s    {\"name\": \"" #id "\", \"macro\": \"" #resp "\", \"code\": %d, \"meaning\": ", sep, resp); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_ERRORS(X)
#undef X
    printf("\n  ],\n  \"verbs\": [");
    sep = "\n";
#define X(id, name, args, reply, meaning) \
    printf("%s    {\"name\": ", sep); \
    jstr(name); \
    printf(", \"arguments\": "); \
    jstr(args); \
    printf(", \"reply\": "); \
    jstr(reply); \
    printf(", \"meaning\": "); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_VERBS(X)
#undef X
    printf("\n  ],\n  \"instance_verbs\": [");
    sep = "\n";
#define X(id, name, args, reply, ledger, meaning) \
    printf("%s    {\"name\": ", sep); \
    jstr(name); \
    printf(", \"arguments\": "); \
    jstr(args); \
    printf(", \"reply\": "); \
    jstr(reply); \
    printf(", \"ledger\": \"%s\", \"meaning\": ", PHD_LEDGER_##ledger == PHD_LEDGER_REPLACE ? "replace" : "none"); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_INSTANCE_VERBS(X)
#undef X
    printf("\n  ],\n  \"host_profiles\": [");
    sep = "\n";
#define X(id, name, meaning) \
    printf("%s    {\"name\": ", sep); \
    jstr(name); \
    printf(", \"meaning\": "); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_HOST_PROFILES(X)
#undef X
    printf("\n  ],\n  \"host_verbs\": [");
    sep = "\n";
#define X(name, source, lv2, clap, console, zynthian) \
    printf("%s    {\"name\": ", sep); \
    jstr(name); \
    printf(", \"source\": "); \
    jstr(PHD_HOST_VERB_SOURCE_##source); \
    printf(", \"levels\": {\"lv2-worker\": \"%s\", \"clap-worker\": \"%s\", \"console\": \"%s\", \"zynthian\": \"%s\"}}", \
           PHD_LEVEL_##lv2, PHD_LEVEL_##clap, PHD_LEVEL_##console, PHD_LEVEL_##zynthian); \
    sep = ",\n";
    PHD_HOST_VERBS(X)
#undef X
    printf("\n  ],\n  \"track_kinds\": [");
    sep = "";
#define X(id, name) printf("%s\"%s\"", sep, name); sep = ", ";
    PHD_TRACK_KINDS(X)
#undef X
    printf("],\n  \"param_scales\": [");
    sep = "\n";
#define X(id, name, meaning) \
    printf("%s    {\"name\": \"%s\", \"meaning\": ", sep, name); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_PARAM_SCALES(X)
#undef X
    printf("\n  ],\n  \"remote_page_slots\": %d,\n  \"string_max\": %d,\n  \"events\": [", PHD_REMOTE_PAGE_SLOTS,
           PHD_STRING_MAX);
    sep = "\n";
#define X(name, fields, meaning) \
    printf("%s    {\"name\": ", sep); \
    jstr(name); \
    printf(", \"fields\": "); \
    jstr(fields); \
    printf(", \"meaning\": "); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_EVENTS(X)
#undef X
    printf("\n  ],\n  \"pin_limits\": [");
    sep = "\n";
#define X(id, meaning) \
    printf("%s    {\"name\": \"" #id "\", \"meaning\": ", sep); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_PIN_LIMITS(X)
#undef X
    printf("\n  ],\n  \"relay\": {\n    \"worker_arguments\": ");
    jstr(PHD_WORKER_ARGUMENTS);
    printf(",\n    \"rules\": [");
    sep = "\n";
#define X(id, meaning) \
    printf("%s      {\"name\": \"" #id "\", \"meaning\": ", sep); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_RELAY(X)
#undef X
    printf("\n    ]\n  },\n  \"host_settings\": [");
    sep = "\n";
#define X(id, meaning) \
    printf("%s    {\"name\": \"" #id "\", \"meaning\": ", sep); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_HOST_SETTINGS(X)
#undef X
    printf("\n  ],\n  \"config\": [");
    sep = "\n";
#define X(key, size, def, env, meaning) \
    printf("%s    {\"key\": \"" #key "\", \"type\": \"string\", \"default\": ", sep); \
    jstr(def); \
    printf(", \"env\": "); \
    jstr(env); \
    printf(", \"meaning\": "); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_CONF_STRINGS(X)
#undef X
#define X(key, def, unit, meaning) \
    printf("%s    {\"key\": \"" #key "\", \"type\": \"int\", \"default\": %d, \"unit\": \"" unit "\", \"meaning\": ", sep, def); \
    jstr(meaning); \
    printf("}"); \
    sep = ",\n";
    PHD_CONF_INTS(X)
#undef X
    printf("\n  ]\n}\n");
}

/* ---------------------------------------------------------------- regions */

/* the host verb table: a row per verb, a column per profile */
static void region_host_verbs(void)
{
    printf("| verb | sent by");
#define X(id, name, meaning) printf(" | %s", name);
    PHD_HOST_PROFILES(X)
#undef X
    printf(" |\n|---|---");
#define X(id, name, meaning) printf("|---");
    PHD_HOST_PROFILES(X)
#undef X
    printf("|\n");
#define X(name, source, lv2, clap, console, zynthian) \
    printf("| `%s` | %s | %s | %s | %s | %s |\n", name, PHD_HOST_VERB_SOURCE_##source, PHD_LEVEL_##lv2, PHD_LEVEL_##clap, \
           PHD_LEVEL_##console, PHD_LEVEL_##zynthian);
    PHD_HOST_VERBS(X)
#undef X
    printf("\nThe profiles: ");
    {
        const char *sep = "";
#define X(id, name, meaning) printf("%s`%s`, %s", sep, name, meaning); sep = "; ";
        PHD_HOST_PROFILES(X)
#undef X
    }
    printf(". `%s` is answered as its grammar says; `%s` is answered, or refused with -902, and never misread; `%s` "
           "is refused on purpose, and no client of the profile may need it.\n", PHD_LEVEL_REQUIRED, PHD_LEVEL_OPTIONAL,
           PHD_LEVEL_ABSENT);
}

static const struct {
    const char *name;
    void (*emit)(void);
} REGIONS[] = {
    { "host-verbs", region_host_verbs },
};

static int splice(const char *path)
{
    char line[4096], name[64];
    FILE *file = fopen(path, "r");
    size_t n;
    int skipping = 0;

    if (!file)
    {
        perror(path);
        return 1;
    }
    while (fgets(line, sizeof(line), file))
    {
        const char *b = strstr(line, "BEGIN GENERATED protocol:"), *e = strstr(line, "END GENERATED protocol:");

        if (skipping)
        {
            if (!e)
                continue;
            skipping = 0;
        }
        else if (b)
        {
            b += strlen("BEGIN GENERATED protocol:");
            /* a name may hold '-': it ends at a space, a newline or the closing of a comment */
            for (n = 0; b[n] && b[n] != ' ' && b[n] != '\n' && n < sizeof(name) - 1; n++)
                name[n] = b[n];
            name[n] = '\0';
            for (n = 0; n < sizeof(REGIONS) / sizeof(*REGIONS); n++)
                if (!strcmp(name, REGIONS[n].name))
                    break;
            if (n == sizeof(REGIONS) / sizeof(*REGIONS))
            {
                fprintf(stderr, "%s: no generated region '%s'\n", path, name);
                return 1;
            }
            fputs(line, stdout);
            REGIONS[n].emit();
            skipping = 1;
            continue;
        }
        fputs(line, stdout);
    }
    fclose(file);
    if (skipping)
    {
        fprintf(stderr, "%s: a generated region is not closed\n", path);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (!scales_are_the_contracts())
        return 1;
    if (argc == 2 && !strcmp(argv[1], "json"))
    {
        json();
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "splice"))
        return splice(argv[2]);
    if (argc == 2 && !strcmp(argv[1], "regions"))
    {
        size_t n;

        for (n = 0; n < sizeof(REGIONS) / sizeof(*REGIONS); n++)
            puts(REGIONS[n].name);
        return 0;
    }
    fprintf(stderr, "usage: protocol-gen json | regions | splice FILE\n");
    return 2;
}
