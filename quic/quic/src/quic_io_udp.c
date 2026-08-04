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

#include <sys/socket.h>

#include <errno.h>
#include <stddef.h>

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

void quic_io_udp_init(quic_io* io, int fd)
{
    QUIC_CHECK(io);
    io->send = io_send;
    io->recv = io_recv;
    io->local_addr = io_local_addr;
    io->fd = io_fd;
    io->io_ctx = (void*)(intptr_t)fd;
}
