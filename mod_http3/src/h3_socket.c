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

#include <apr_portable.h>
#include <unistd.h>

#if defined(_WIN32)
    #include <winsock2.h>
    #define H3_STATUS_IS_EADDRINUSE(s) ((s) == APR_FROM_OS_ERROR(WSAEADDRINUSE))
#else
    #include <errno.h>
    #define H3_STATUS_IS_EADDRINUSE(s) ((s) == APR_FROM_OS_ERROR(EADDRINUSE))
#endif

#include "h3_check.h"
#include "h3_socket.h"

/*
 * A UDP socket left at the OS default receive buffer (commonly 208KB) starts
 * dropping datagrams as soon as one QUIC connection runs at speed, and every
 * drop costs a retransmit and a congestion-window cut. The kernel caps what it
 * grants (net.core.rmem_max / wmem_max on Linux), so this asks and reports
 * what it got; it never fails the socket over a buffer size.
 */
static void tune_buffers(apr_socket_t* sock, apr_size_t want, apr_port_t port, apr_pool_t* pool)
{
    if (want == 0 || want > (apr_size_t)APR_INT32_MAX)
    {
        return;
    }
    static const apr_int32_t opts[] = {APR_SO_RCVBUF, APR_SO_SNDBUF};
    static const char* const names[] = {"SO_RCVBUF", "SO_SNDBUF"};
    for (int i = 0; i < 2; i++)
    {
        if (apr_socket_opt_set(sock, opts[i], (apr_int32_t)want) != APR_SUCCESS)
        {
            ap_log_perror(APLOG_MARK, APLOG_INFO, 0, pool, "h3_socket_open(%d): %s could not be set to %" APR_SIZE_T_FMT " bytes, keeping the OS default", (int)port, names[i], want);
            continue;
        }
        apr_int32_t got = 0;
        if (apr_socket_opt_get(sock, opts[i], &got) == APR_SUCCESS && (apr_size_t)got < want)
        {
            ap_log_perror(APLOG_MARK, APLOG_INFO, 0, pool, "h3_socket_open(%d): %s capped at %d bytes of the %" APR_SIZE_T_FMT " requested; raise the OS limit to grant more", (int)port, names[i], (int)got, want);
        }
    }
}

apr_status_t h3_socket_open(apr_port_t port, apr_size_t buffer_size, apr_pool_t* pool, int* out_fd)
{
    CHECK(pool);
    CHECK(out_fd);
    apr_socket_t* sock = NULL;
    apr_status_t rv = apr_socket_create(&sock, APR_INET6, SOCK_DGRAM, APR_PROTO_UDP, pool);
    if (rv != APR_SUCCESS)
    {
        ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool, "h3_socket_open: apr_socket_create failed");
        return rv;
    }
    apr_socket_opt_set(sock, APR_IPV6_V6ONLY, 0);
    tune_buffers(sock, buffer_size, port, pool);
    apr_sockaddr_t* addr = NULL;
    rv = apr_sockaddr_info_get(&addr, NULL, APR_INET6, port, 0, pool);
    if (rv != APR_SUCCESS)
    {
        apr_socket_close(sock);
        ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool, "h3_socket_open: apr_sockaddr_info_get failed");
        return rv;
    }
    rv = apr_socket_bind(sock, addr);
    if (rv != APR_SUCCESS)
    {
        apr_socket_close(sock);
        if (H3_STATUS_IS_EADDRINUSE(rv))
        {
            ap_log_perror(APLOG_MARK, APLOG_DEBUG, 0, pool, "bind(%d) skipped (port already owned)", (int)port);
            return APR_EAGAIN;
        }
        ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool, "bind(%d) failed", (int)port);
        return rv;
    }
    rv = apr_socket_timeout_set(sock, 0);
    if (rv != APR_SUCCESS)
    {
        apr_socket_close(sock);
        ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool, "set nonblocking on %d failed", (int)port);
        return rv;
    }
    apr_os_sock_t os_sock;
    rv = apr_os_sock_get(&os_sock, sock);
    if (rv != APR_SUCCESS)
    {
        apr_socket_close(sock);
        ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool, "h3_socket_open: apr_os_sock_get failed");
        return rv;
    }
    *out_fd = (int)os_sock;
    return APR_SUCCESS;
}

void h3_socket_close(int fd)
{
    if (fd >= 0)
    {
#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif
    }
}
