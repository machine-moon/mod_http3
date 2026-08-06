/*
 * Copyright (c) 2026 The mod_http3 Project Authors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sput.h"

#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string.h>

#include "quic.h"

#define DGRAM_CAP 2048

/* Bound loopback UDP socket pair: [0] receives, [1] sends to it. */
static int make_pair(int fds[2], struct sockaddr_in* to)
{
    fds[0] = socket(AF_INET, SOCK_DGRAM, 0);
    fds[1] = socket(AF_INET, SOCK_DGRAM, 0);
    if (fds[0] < 0 || fds[1] < 0)
    {
        return 0;
    }
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)}, .sin_port = 0};
    if (bind(fds[0], (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        return 0;
    }
    socklen_t len = sizeof(*to);
    return getsockname(fds[0], (struct sockaddr*)to, &len) == 0;
}

static void close_pair(int fds[2])
{
    close(fds[0]);
    close(fds[1]);
}

/* Datagrams cross loopback asynchronously; wait rather than race the kernel. */
static int wait_readable(int fd)
{
    struct pollfd p = {.fd = fd, .events = POLLIN};
    return poll(&p, 1, 2000) == 1;
}

static void test_recv_batch_reads_every_queued_datagram(void)
{
    int fds[2];
    struct sockaddr_in to;
    if (!make_pair(fds, &to))
    {
        sput_fail_unless(0, "loopback UDP socket pair could be created");
        return;
    }

    quic_io io;
    memset(&io, 0, sizeof(io));
    quic_io_udp_init(&io, fds[0]);
    if (!io.recv_batch)
    {
        /* No batch read on this platform; the engines use recv() instead. */
        close_pair(fds);
        return;
    }

    static const char* payloads[] = {"first", "second-datagram", "3"};
    for (int i = 0; i < 3; i++)
    {
        sendto(fds[1], payloads[i], strlen(payloads[i]), 0, (struct sockaddr*)&to, sizeof(to));
    }
    sput_fail_unless(wait_readable(fds[0]), "the receiving socket becomes readable");

    uint8_t bufs[QUIC_IO_RECV_BATCH][DGRAM_CAP];
    quic_dgram dgrams[QUIC_IO_RECV_BATCH];
    for (size_t i = 0; i < QUIC_IO_RECV_BATCH; i++)
    {
        dgrams[i].base = bufs[i];
        dgrams[i].len = DGRAM_CAP;
    }

    quic_ssize n = io.recv_batch(io.io_ctx, dgrams, QUIC_IO_RECV_BATCH);
    sput_fail_unless(n == 3, "one batch read returns all three queued datagrams");

    if (n == 3)
    {
        int matched = 1;
        for (int i = 0; i < 3; i++)
        {
            matched = matched && dgrams[i].len == strlen(payloads[i]) && memcmp(dgrams[i].base, payloads[i], dgrams[i].len) == 0;
        }
        sput_fail_unless(matched, "each datagram keeps its own payload and length, in order");
        sput_fail_unless(dgrams[0].peer_len > 0, "the sender address is reported per datagram");
    }

    close_pair(fds);
}

static void test_recv_batch_reports_empty_socket(void)
{
    int fds[2];
    struct sockaddr_in to;
    if (!make_pair(fds, &to))
    {
        sput_fail_unless(0, "loopback UDP socket pair could be created");
        return;
    }

    quic_io io;
    memset(&io, 0, sizeof(io));
    quic_io_udp_init(&io, fds[0]);
    if (!io.recv_batch)
    {
        close_pair(fds);
        return;
    }

    uint8_t buf[DGRAM_CAP];
    quic_dgram dgram = {.base = buf, .len = sizeof(buf)};
    quic_ssize n = io.recv_batch(io.io_ctx, &dgram, 1);
    sput_fail_unless(n <= 0, "a batch read on a drained socket reports no datagrams instead of blocking");

    close_pair(fds);
}

static void test_recv_batch_drops_a_datagram_that_does_not_fit(void)
{
    int fds[2];
    struct sockaddr_in to;
    if (!make_pair(fds, &to))
    {
        sput_fail_unless(0, "loopback UDP socket pair could be created");
        return;
    }

    quic_io io;
    memset(&io, 0, sizeof(io));
    quic_io_udp_init(&io, fds[0]);
    if (!io.recv_batch)
    {
        close_pair(fds);
        return;
    }

    /* Larger than the slot: a half-read packet is not decodable as QUIC, so the
     * batch must report it empty and keep going rather than hand up a prefix. */
    static uint8_t big[1200];
    memset(big, 'x', sizeof(big));
    sendto(fds[1], big, sizeof(big), 0, (struct sockaddr*)&to, sizeof(to));
    sput_fail_unless(wait_readable(fds[0]), "the receiving socket becomes readable");

    uint8_t small[64];
    quic_dgram dgram = {.base = small, .len = sizeof(small)};
    quic_ssize n = io.recv_batch(io.io_ctx, &dgram, 1);
    sput_fail_unless(n == 1, "the oversized datagram is still consumed from the socket");
    if (n == 1)
    {
        sput_fail_unless(dgram.len == 0, "a truncated datagram is reported empty so the caller skips it");
    }

    close_pair(fds);
}

void run_quic_io_udp_tests(void)
{
    sput_run_test(test_recv_batch_reads_every_queued_datagram);
    sput_run_test(test_recv_batch_reports_empty_socket);
    sput_run_test(test_recv_batch_drops_a_datagram_that_does_not_fit);
}
