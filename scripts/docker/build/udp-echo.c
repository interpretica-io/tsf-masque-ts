/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/*
 * Trivial UDP echo server for the MASQUE suite's target.
 * Usage: udp-echo <ip> <port>
 * Echoes every datagram back to its sender, verbatim.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int
main(int argc, char **argv)
{
    struct sockaddr_in addr;
    struct sockaddr_in peer;
    socklen_t peer_len;
    unsigned char buf[65535];
    int fd;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return perror("socket"), 1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1)
        return fprintf(stderr, "bad ip %s\n", argv[1]), 1;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return perror("bind"), 1;

    for (;;)
    {
        ssize_t n;

        peer_len = sizeof(peer);
        n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&peer,
                     &peer_len);
        if (n < 0)
            continue;
        (void)sendto(fd, buf, n, 0, (struct sockaddr *)&peer, peer_len);
    }
    return 0;
}
