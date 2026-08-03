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
#include "detail/quic_ngtcp2_impl.h"
#include "detail/quic_tls.h"

SSL_CTX* quic_ngtcp2_tls_ctx_create(const quic_config* cfg, char* err, size_t errlen)
{
    QUIC_CHECK(cfg);

    if (ngtcp2_crypto_ossl_init() != 0)
    {
        quic_tls_error(err, errlen, "ngtcp2_crypto_ossl_init failed; OpenSSL lacks the QUIC TLS API");
        return NULL;
    }
    return quic_tls_ctx_create(TLS_server_method(), cfg, err, errlen);
}

static ngtcp2_conn* conn_ref_get_conn(ngtcp2_crypto_conn_ref* conn_ref)
{
    quic_ngtcp2_conn* conn = conn_ref->user_data;
    return conn->qconn;
}

int quic_ngtcp2_tls_session_init(quic_ngtcp2_conn* conn)
{
    QUIC_CHECK(conn);

    if (ngtcp2_crypto_ossl_ctx_new(&conn->ossl_ctx, NULL) != 0)
    {
        return 0;
    }

    conn->ssl = SSL_new(conn->engine->ssl_ctx);
    if (!conn->ssl)
    {
        ngtcp2_crypto_ossl_ctx_del(conn->ossl_ctx);
        conn->ossl_ctx = NULL;
        return 0;
    }

    ngtcp2_crypto_ossl_ctx_set_ssl(conn->ossl_ctx, conn->ssl);
    if (ngtcp2_crypto_ossl_configure_server_session(conn->ssl) != 0)
    {
        quic_ngtcp2_tls_session_free(conn);
        return 0;
    }

    conn->conn_ref.get_conn = conn_ref_get_conn;
    conn->conn_ref.user_data = conn;
    SSL_set_app_data(conn->ssl, &conn->conn_ref);
    SSL_set_accept_state(conn->ssl);

    ngtcp2_conn_set_tls_native_handle(conn->qconn, conn->ossl_ctx);
    return 1;
}

void quic_ngtcp2_tls_session_free(quic_ngtcp2_conn* conn)
{
    if (!conn)
    {
        return;
    }
    if (conn->ssl)
    {
        SSL_set_app_data(conn->ssl, NULL);
        SSL_free(conn->ssl);
        conn->ssl = NULL;
    }
    if (conn->ossl_ctx)
    {
        ngtcp2_crypto_ossl_ctx_del(conn->ossl_ctx);
        conn->ossl_ctx = NULL;
    }
}
