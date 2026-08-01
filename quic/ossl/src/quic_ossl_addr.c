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

#include <netinet/in.h>

#include <string.h>

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include "detail/quic_check.h"
#include "detail/quic_ossl_impl.h"
#include "quic.h"

struct quic_ossl_datagram
{
    unsigned char* data;
    size_t data_len;
    BIO_ADDR* peer;
    BIO_ADDR* local;
    quic_ossl_datagram* next;
};

void quic_ossl_peer_addr_queue_clear(quic_engine* engine)
{
    quic_ossl_datagram* item = engine->peer_rx_head;
    while (item)
    {
        quic_ossl_datagram* next = item->next;
        OPENSSL_free(item->data);
        BIO_ADDR_free(item->peer);
        BIO_ADDR_free(item->local);
        OPENSSL_free(item);
        item = next;
    }
    engine->peer_rx_head = NULL;
    engine->peer_rx_tail = NULL;
}

static int quic_ossl_queue_fill(quic_engine* engine, BIO_MSG* msg, size_t stride, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        BIO_MSG* source = (BIO_MSG*)((unsigned char*)msg + i * stride);
        quic_ossl_datagram* item = OPENSSL_zalloc(sizeof(*item));
        if (!item || !source->data || source->data_len == 0)
        {
            OPENSSL_free(item);
            quic_ossl_peer_addr_queue_clear(engine);
            return 0;
        }
        item->data = OPENSSL_memdup(source->data, source->data_len);
        item->data_len = source->data_len;
        item->peer = source->peer ? BIO_ADDR_dup(source->peer) : NULL;
        item->local = source->local ? BIO_ADDR_dup(source->local) : NULL;
        if (!item->data || (source->peer && !item->peer) || (source->local && !item->local))
        {
            OPENSSL_free(item->data);
            BIO_ADDR_free(item->peer);
            BIO_ADDR_free(item->local);
            OPENSSL_free(item);
            quic_ossl_peer_addr_queue_clear(engine);
            return 0;
        }
        if (engine->peer_rx_tail)
        {
            engine->peer_rx_tail->next = item;
        }
        else
        {
            engine->peer_rx_head = item;
        }
        engine->peer_rx_tail = item;
    }
    return 1;
}

static int quic_ossl_queue_pop(quic_engine* engine, BIO_MSG* msg)
{
    quic_ossl_datagram* item = engine->peer_rx_head;
    if (!item || !msg || !msg->data || msg->data_len < item->data_len)
    {
        return 0;
    }
    memcpy(msg->data, item->data, item->data_len);
    msg->data_len = item->data_len;
    if (msg->peer && item->peer)
    {
        BIO_ADDR_copy(msg->peer, item->peer);
    }
    if (msg->local && item->local)
    {
        BIO_ADDR_copy(msg->local, item->local);
    }
    engine->peer_rx_head = item->next;
    if (!engine->peer_rx_head)
    {
        engine->peer_rx_tail = NULL;
    }
    OPENSSL_free(item->data);
    BIO_ADDR_free(item->peer);
    BIO_ADDR_free(item->local);
    OPENSSL_free(item);
    return 1;
}

long quic_ossl_peer_addr_bio_ctrl(BIO* bio, int cmd, long num, void* ptr)
{
    BIO* next = BIO_next(bio);
    return next ? BIO_ctrl(next, cmd, num, ptr) : 0;
}

int quic_ossl_peer_addr_bio_sendmmsg(BIO* bio, BIO_MSG* msg, size_t stride, size_t num_msg, uint64_t flags, size_t* msgs_processed)
{
    BIO* next = BIO_next(bio);
    return next ? BIO_sendmmsg(next, msg, stride, num_msg, flags, msgs_processed) : 0;
}

int quic_ossl_peer_addr_bio_recvmmsg(BIO* bio, BIO_MSG* msg, size_t stride, size_t num_msg, uint64_t flags, size_t* msgs_processed)
{
    quic_engine* engine = BIO_get_data(bio);
    BIO* next = BIO_next(bio);
    if (!engine || !next || !msg || !msgs_processed || num_msg == 0)
    {
        return 0;
    }

    BIO_ADDR_clear(engine->current_peer_addr);
    if (engine->peer_rx_head)
    {
        *msgs_processed = 0;
        if (!quic_ossl_queue_pop(engine, msg))
        {
            return 0;
        }
        *msgs_processed = 1;
        if (msg->peer)
        {
            BIO_ADDR_copy(engine->current_peer_addr, msg->peer);
        }
        return 1;
    }

    size_t received = 0;
    int rv = BIO_recvmmsg(next, msg, stride, num_msg, flags, &received);
    if (rv && received > 0)
    {
        if (!quic_ossl_queue_fill(engine, msg, stride, received) || !quic_ossl_queue_pop(engine, msg))
        {
            *msgs_processed = 0;
            return 0;
        }
        *msgs_processed = 1;
        if (msg->peer)
        {
            BIO_ADDR_copy(engine->current_peer_addr, msg->peer);
        }
    }
    else
    {
        *msgs_processed = received;
    }
    return rv;
}

int quic_ossl_peer_addr_bio_destroy(BIO* bio)
{
    quic_engine* engine = BIO_get_data(bio);
    if (engine)
    {
        quic_ossl_peer_addr_queue_clear(engine);
    }
    return 1;
}

void quic_ossl_peer_addr_ex_free(void* /*parent*/, void* ptr, CRYPTO_EX_DATA* /*ad*/, int /*idx*/, long /*argl*/, void* /*argp*/)
{
    BIO_ADDR_free(ptr);
}

int quic_ossl_new_pending_conn_cb(SSL_CTX* /*ctx*/, SSL* conn, void* arg)
{
    quic_engine* engine = arg;
    if (!engine || engine->peer_addr_ex_index < 0 || BIO_ADDR_family(engine->current_peer_addr) == AF_UNSPEC)
    {
        return 1;
    }

    BIO_ADDR* peer = BIO_ADDR_dup(engine->current_peer_addr);
    if (!peer || !SSL_set_ex_data(conn, engine->peer_addr_ex_index, peer))
    {
        BIO_ADDR_free(peer);
        return 0;
    }
    return 1;
}

int quic_ossl_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len)
{
    QUIC_CHECK(engine);
    QUIC_CHECK(conn);
    QUIC_CHECK(addr);
    QUIC_CHECK(addr_len);
    if (engine->peer_addr_ex_index < 0)
    {
        return 0;
    }

    const BIO_ADDR* peer = SSL_get_ex_data((SSL*)conn, engine->peer_addr_ex_index);
    if (!peer)
    {
        return 0;
    }

    memset(addr, 0, sizeof(*addr));
    size_t rawlen = 0;
    int family = BIO_ADDR_family(peer);
    if (family == AF_INET)
    {
        struct sockaddr_in* sin = (struct sockaddr_in*)addr;
        if (!BIO_ADDR_rawaddress(peer, &sin->sin_addr, &rawlen) || rawlen != sizeof(sin->sin_addr))
        {
            return 0;
        }
        sin->sin_family = AF_INET;
        sin->sin_port = BIO_ADDR_rawport(peer);
        *addr_len = sizeof(*sin);
        return 1;
    }
    if (family == AF_INET6)
    {
        struct sockaddr_in6* sin6 = (struct sockaddr_in6*)addr;
        if (!BIO_ADDR_rawaddress(peer, &sin6->sin6_addr, &rawlen) || rawlen != sizeof(sin6->sin6_addr))
        {
            return 0;
        }
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = BIO_ADDR_rawport(peer);
        *addr_len = sizeof(*sin6);
        return 1;
    }
    return 0;
}
