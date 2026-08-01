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

#include <stdlib.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>

#include "detail/quic_check.h"
#include "detail/quic_ossl_impl.h"
#include "detail/quic_tls.h"
#include "quic.h"
#include "quic_ossl.h"

quic_engine* quic_ossl_engine_create(const quic_config* cfg, int udp_fd, char* err, size_t errlen)
{
    QUIC_CHECK(cfg);
    quic_engine* engine = calloc(1, sizeof(*engine));
    if (!engine)
    {
        quic_tls_error(err, errlen, "allocating the engine failed");
        return NULL;
    }
    engine->peer_addr_ex_index = -1;

    engine->ssl_ctx = quic_tls_ctx_create(OSSL_QUIC_server_method(), cfg, err, errlen);
    if (!engine->ssl_ctx)
    {
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    BIO_METHOD* bm = BIO_meth_new(BIO_TYPE_FILTER | BIO_get_new_index(), "quic_ossl_peer_addr");
    if (!bm)
    {
        quic_tls_error(err, errlen, "BIO_meth_new failed");
        quic_ossl_engine_destroy(engine);
        return NULL;
    }
    BIO_meth_set_ctrl(bm, quic_ossl_peer_addr_bio_ctrl);
    BIO_meth_set_sendmmsg(bm, quic_ossl_peer_addr_bio_sendmmsg);
    BIO_meth_set_recvmmsg(bm, quic_ossl_peer_addr_bio_recvmmsg);
    BIO_meth_set_destroy(bm, quic_ossl_peer_addr_bio_destroy);
    engine->peer_addr_bio_method = bm;

    engine->current_peer_addr = BIO_ADDR_new();
    engine->peer_addr_ex_index = SSL_get_ex_new_index(0, NULL, NULL, NULL, quic_ossl_peer_addr_ex_free);
    if (!engine->current_peer_addr || engine->peer_addr_ex_index < 0)
    {
        quic_tls_error(err, errlen, "initializing peer address recovery failed");
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    SSL_CTX_set_new_pending_conn_cb(engine->ssl_ctx, quic_ossl_new_pending_conn_cb, engine);

    uint64_t listener_flags = cfg->address_validation ? 0 : (uint64_t)SSL_LISTENER_FLAG_NO_VALIDATE;
    engine->ssl_listener = SSL_new_listener(engine->ssl_ctx, listener_flags);
    if (!engine->ssl_listener)
    {
        quic_tls_error(err, errlen, "SSL_new_listener failed");
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    BIO* bio = BIO_new_dgram(udp_fd, BIO_NOCLOSE);
    if (!bio)
    {
        quic_tls_error(err, errlen, "BIO_new_dgram failed for fd=%d", udp_fd);
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    BIO* filter_bio = BIO_new(bm);
    if (!filter_bio)
    {
        quic_tls_error(err, errlen, "BIO_new(quic_ossl_peer_addr) failed");
        BIO_free(bio);
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    BIO_set_data(filter_bio, engine);
    bio = BIO_push(filter_bio, bio);
    SSL_set_bio(engine->ssl_listener, bio, bio);

    if (!SSL_listen(engine->ssl_listener) || !SSL_set_blocking_mode(engine->ssl_listener, 0))
    {
        quic_tls_error(err, errlen, "SSL_listen failed");
        quic_ossl_engine_destroy(engine);
        return NULL;
    }

    return engine;
}

void quic_ossl_engine_destroy(quic_engine* engine)
{
    if (!engine)
    {
        return;
    }
    quic_ossl_peer_addr_queue_clear(engine);
    if (engine->ssl_listener)
    {
        SSL_free(engine->ssl_listener);
    }
    if (engine->current_peer_addr)
    {
        BIO_ADDR_free(engine->current_peer_addr);
    }
    if (engine->peer_addr_bio_method)
    {
        BIO_meth_free(engine->peer_addr_bio_method);
    }
    if (engine->ssl_ctx)
    {
        SSL_CTX_free(engine->ssl_ctx);
    }
    free(engine);
}

const char* quic_ossl_engine_last_error(quic_engine* engine)
{
    if (!engine || !engine->err_pending)
    {
        return "";
    }
    engine->err_pending = 0;
    return engine->err;
}

void quic_ossl_engine_socket_configure(quic_engine* engine, int fd)
{
    (void)engine;
    (void)fd;
}

int quic_ossl_engine_pump(quic_engine* engine)
{
    if (!engine || !engine->ssl_listener)
    {
        return 0;
    }
    int work = 0;
    SSL_handle_events(engine->ssl_listener);
    while (engine->peer_rx_head)
    {
        if (SSL_handle_events(engine->ssl_listener) != 1)
        {
            break;
        }
        work = 1;
    }
    return work;
}

void quic_ossl_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms)
{
    if (!engine || !engine->ssl_listener)
    {
        *want_read = 0;
        *want_write = 0;
        *timeout_ms = 1000;
        return;
    }
    *want_read = SSL_net_read_desired(engine->ssl_listener);
    *want_write = SSL_net_write_desired(engine->ssl_listener);

    struct timeval tv = {0};
    int is_infinite = 0;
    if (SSL_get_event_timeout(engine->ssl_listener, &tv, &is_infinite) && !is_infinite)
    {
        long ms = (long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
        if (ms < *timeout_ms)
        {
            *timeout_ms = (int)ms;
        }
    }
    if (*timeout_ms < 0)
    {
        *timeout_ms = 0;
    }
}

quic_conn* quic_ossl_engine_accept_conn(quic_engine* engine)
{
    if (!engine || !engine->ssl_listener)
    {
        return NULL;
    }
    SSL* conn = SSL_accept_connection(engine->ssl_listener, SSL_ACCEPT_CONNECTION_NO_BLOCK);
    return (quic_conn*)conn;
}
