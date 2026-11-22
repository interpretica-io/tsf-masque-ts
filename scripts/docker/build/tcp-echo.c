/* SPDX-License-Identifier: MIT */
/* Copyright (C) 2026 Interpretica, Unipessoal Lda. All rights reserved. */
/*
 * Trivial TCP echo server for the MASQUE suite's target.
 * Usage: tcp-echo <ip> <port>
 * Accepts connections and echoes each one's byte stream back until close.
 * One connection at a time via fork().
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

int
main(int argc, char **argv)
{
    struct sockaddr_in addr;
    int fd;
    int one = 1;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }
    signal(SIGCHLD, SIG_IGN);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return perror("socket"), 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1)
        return fprintf(stderr, "bad ip %s\n", argv[1]), 1;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
        return perror("bind"), 1;
    if (listen(fd, 16) != 0)
        return perror("listen"), 1;

    for (;;)
    {
        int c = accept(fd, NULL, NULL);

        if (c < 0)
            continue;
        if (fork() == 0)
        {
            unsigned char buf[65535];
            ssize_t n;

            close(fd);
            while ((n = read(c, buf, sizeof(buf))) > 0)
            {
                ssize_t off = 0;

                while (off < n)
                {
                    ssize_t w = write(c, buf + off, n - off);

                    if (w <= 0)
                        break;
                    off += w;
                }
            }
            close(c);
            _exit(0);
        }
        close(c);
    }
    return 0;
}
