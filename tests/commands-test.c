/*
 * How many commands an installed libplugin-hostd-protocol takes (the carried protocol-max-commands patch).
 *
 * A host that registers more commands than the table holds lost the last ones in silence: omx-clap-host's 65th was
 * quit. The library now holds 128 and refuses one past them with -1. This registers 65 and checks each is taken,
 * which a library that holds 64 fails (and one whose protocol_add_command returns nothing does not build against).
 * Built against the installed headers and library: tests/install-check.sh.
 */

#include <protocol.h>

#include <stdio.h>

static void callback(proto_t *proto)
{
    (void)proto;
}

int main(void)
{
    static char names[65][32];
    int i;

    for (i = 0; i < 65; i++)
    {
        snprintf(names[i], sizeof(names[i]), "verb_%d %%i", i);
        if (protocol_add_command(names[i], callback) != 0)
        {
            printf("FAILED  command %d of 65 was not registered\n", i + 1);
            return 1;
        }
    }
    printf("ok      65 commands registered\n");
    return 0;
}
