/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Who reaches the ports an installed libplugin-hostd-protocol opens (the carried bind-address patch).
 *
 * A host that links the library and calls socket_start() without choosing an address, as omx-clap-host does, must be
 * reached from this machine's 127.0.0.1 and from nowhere else. 127.0.0.2 is also this machine and a port bound to
 * 127.0.0.1 alone refuses it, so that arm runs on a host with no network; an address off the loopback, when there is
 * one, is the real case. A library without the patch listens on every address and fails this, or does not link.
 * Built against the installed headers and library: tests/install-check.sh.
 */

#include <socket.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int reaches(const char *address, int port)
{
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int rc;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address, &addr.sin_addr);
    rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    close(fd);
    return rc == 0;
}

static int check(const char *address, int port, int want)
{
    const int got = reaches(address, port);

    printf("%s  default bind: %s:%d %s\n", got == want ? "ok    " : "FAILED", address, port, got ? "reached" : "refused");
    return got != want;
}

int main(void)
{
    static char text[INET_ADDRSTRLEN];
    const char *nic = NULL;
    struct ifaddrs *all, *ifa;
    int port, failures = 0;

    if (getifaddrs(&all) == 0)
    {
        for (ifa = all; ifa && !nic; ifa = ifa->ifa_next)
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && (ifa->ifa_flags & IFF_UP) &&
                !(ifa->ifa_flags & IFF_LOOPBACK))
                nic = inet_ntop(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, text, sizeof(text));
        freeifaddrs(all);
    }
    printf("%s\n", nic ? nic : "NOT RUN off the loopback: this machine has no such address, 127.0.0.2 stands for it");

    for (port = 30000 + getpid() % 20000; port < 65000; port += 2)
        if (socket_start(port, port + 1, 1024) == 0)
            break;
    if (port >= 65000)
    {
        printf("FAILED  socket_start: no free ports\n");
        return 1;
    }
    failures += check("127.0.0.1", port, 1);
    failures += check("127.0.0.1", port + 1, 1);
    failures += check("127.0.0.2", port, 0);
    failures += check("127.0.0.2", port + 1, 0);
    if (nic)
    {
        failures += check(nic, port, 0);
        failures += check(nic, port + 1, 0);
    }
    socket_finish();
    if (failures)
        printf("FAILED  bind: %d checks\n", failures);
    return failures != 0;
}
