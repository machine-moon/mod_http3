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

#include <openssl/ssl.h>

#include "detail/quic_check.h"
#include "detail/quic_ossl_impl.h"
#include "quic.h"

int quic_ossl_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs)
{
    SSL* ssl_conn = (SSL*)conn;
    if (!ssl_conn || !SSL_set_blocking_mode(ssl_conn, 0))
    {
        return 0;
    }
    SSL_set_default_stream_mode(ssl_conn, SSL_DEFAULT_STREAM_MODE_NONE);
    SSL_set_incoming_stream_policy(ssl_conn, SSL_INCOMING_STREAM_POLICY_ACCEPT, 0);
    SSL_set_generic_value_uint(ssl_conn, SSL_VALUE_QUIC_IDLE_TIMEOUT, (uint64_t)idle_timeout_secs * 1000);
    return 1;
}

quic_stream* quic_ossl_conn_open_uni_stream(quic_conn* conn, int64_t* out_id)
{
    SSL* ssl_conn = (SSL*)conn;
    QUIC_CHECK(ssl_conn);
    QUIC_CHECK(out_id);
    SSL* stream = SSL_new_stream(ssl_conn, SSL_STREAM_FLAG_UNI);
    if (!stream)
    {
        return NULL;
    }
    *out_id = (int64_t)SSL_get_stream_id(stream);
    return (quic_stream*)stream;
}

quic_stream* quic_ossl_conn_accept_stream(quic_conn* conn)
{
    SSL* ssl_conn = (SSL*)conn;
    if (!ssl_conn)
    {
        return NULL;
    }
    return (quic_stream*)SSL_accept_stream(ssl_conn, SSL_ACCEPT_STREAM_NO_BLOCK);
}

int quic_ossl_conn_is_handshake_done(quic_conn* conn)
{
    SSL* ssl_conn = (SSL*)conn;
    return ssl_conn ? SSL_is_init_finished(ssl_conn) : 0;
}

int quic_ossl_conn_is_closed(quic_conn* conn)
{
    SSL* ssl_conn = (SSL*)conn;
    return ssl_conn ? (SSL_get_shutdown(ssl_conn) != 0) : 1;
}

int quic_ossl_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason)
{
    SSL* ssl_conn = (SSL*)conn;
    if (!ssl_conn)
    {
        return 1;
    }
    uint64_t flags = is_rapid ? (uint64_t)SSL_SHUTDOWN_FLAG_RAPID : 0;
    int ret = 0;
    if (reason)
    {
        SSL_SHUTDOWN_EX_ARGS args = {.quic_error_code = app_error, .quic_reason = reason};
        ret = SSL_shutdown_ex(ssl_conn, flags, &args, sizeof(args));
    }
    else if (flags != 0)
    {
        SSL_SHUTDOWN_EX_ARGS args = {0};
        ret = SSL_shutdown_ex(ssl_conn, flags, &args, sizeof(args));
    }
    else
    {
        ret = SSL_shutdown(ssl_conn);
    }

    if (ret == 1)
    {
        return 1;
    }
    if (ret < 0)
    {
        int err = SSL_get_error(ssl_conn, ret);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
        {
            return 1;
        }
    }
    return 0;
}

void quic_ossl_conn_free(quic_conn* conn)
{
    if (conn)
    {
        SSL_free((SSL*)conn);
    }
}
