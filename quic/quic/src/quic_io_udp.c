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

/* recvmmsg() is a GNU extension, and this must precede the libc headers. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

#include <sys/socket.h>

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "detail/quic_check.h"
#include "quic.h"

/* The fd is borrowed: the caller opened it and closes it. */
static int io_fd(void* io_ctx)
{
    return (int)(intptr_t)io_ctx;
}

static quic_ssize io_send(void* io_ctx, const uint8_t* buf, size_t len, const struct sockaddr* to, socklen_t to_len)
{
    ssize_t n;
    do
    {
        n = sendto(io_fd(io_ctx), buf, len, 0, to, to_len);
    } while (n < 0 && errno == EINTR);
    return (quic_ssize)n;
}

static quic_ssize io_recv(void* io_ctx, uint8_t* buf, size_t cap, struct sockaddr_storage* from, socklen_t* from_len)
{
    ssize_t n;
    *from_len = (socklen_t)sizeof(*from);
    do
    {
        n = recvfrom(io_fd(io_ctx), buf, cap, 0, (struct sockaddr*)from, from_len);
    } while (n < 0 && errno == EINTR);
    return (quic_ssize)n;
}

static int io_local_addr(void* io_ctx, struct sockaddr_storage* addr, socklen_t* addr_len)
{
    *addr_len = (socklen_t)sizeof(*addr);
    return getsockname(io_fd(io_ctx), (struct sockaddr*)addr, addr_len) == 0;
}

#if defined(__linux__)
static quic_ssize io_recv_batch(void* io_ctx, quic_dgram* dgrams, size_t ndgrams)
{
    if (ndgrams > QUIC_IO_RECV_BATCH)
    {
        ndgrams = QUIC_IO_RECV_BATCH;
    }

    struct mmsghdr msgs[QUIC_IO_RECV_BATCH];
    struct iovec iov[QUIC_IO_RECV_BATCH];
    memset(msgs, 0, sizeof(msgs[0]) * ndgrams);
    for (size_t i = 0; i < ndgrams; i++)
    {
        iov[i].iov_base = dgrams[i].base;
        iov[i].iov_len = dgrams[i].len;
        msgs[i].msg_hdr.msg_iov = &iov[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_name = &dgrams[i].peer;
        msgs[i].msg_hdr.msg_namelen = (socklen_t)sizeof(dgrams[i].peer);
    }

    int n;
    do
    {
        n = recvmmsg(io_fd(io_ctx), msgs, (unsigned int)ndgrams, MSG_DONTWAIT, NULL);
    } while (n < 0 && errno == EINTR);

    if (n < 0)
    {
        return -1;
    }
    for (int i = 0; i < n; i++)
    {
        /* A truncated datagram is not a parseable QUIC packet; report it empty
         * so the caller drops just that one and keeps the rest of the batch. */
        dgrams[i].len = (msgs[i].msg_hdr.msg_flags & MSG_TRUNC) ? 0 : msgs[i].msg_len;
        dgrams[i].peer_len = msgs[i].msg_hdr.msg_namelen;
    }
    return (quic_ssize)n;
}
#endif /* __linux__ */

void quic_io_udp_init(quic_io* io, int fd)
{
    QUIC_CHECK(io);
    io->send = io_send;
    io->recv = io_recv;
    io->local_addr = io_local_addr;
    io->fd = io_fd;
    io->io_ctx = (void*)(intptr_t)fd;
#if defined(__linux__)
    io->recv_batch = io_recv_batch;
#else
    io->recv_batch = NULL;
#endif
}
