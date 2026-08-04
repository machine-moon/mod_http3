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
#include <string.h>

#include <openssl/rand.h>

#include "detail/quic_check.h"
#include "detail/quic_ngtcp2_impl.h"

static int cb_handshake_completed(ngtcp2_conn* qconn, void* user_data)
{
    (void)qconn;
    quic_ngtcp2_conn* conn = user_data;
    conn->handshake_done = 1;
    return 0;
}

static int cb_recv_stream_data(ngtcp2_conn* qconn, uint32_t flags, int64_t stream_id, uint64_t offset, const uint8_t* data, size_t datalen, void* user_data, void* stream_user_data)
{
    (void)qconn;
    (void)offset;
    (void)stream_user_data;
    quic_ngtcp2_conn* conn = user_data;
    if (!quic_ngtcp2_stream_recv(conn, stream_id, data, datalen, (flags & NGTCP2_STREAM_DATA_FLAG_FIN) != 0))
    {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

static int cb_acked_stream_data_offset(ngtcp2_conn* qconn, int64_t stream_id, uint64_t offset, uint64_t datalen, void* user_data, void* stream_user_data)
{
    (void)qconn;
    (void)offset;
    (void)stream_user_data;
    quic_ngtcp2_conn* conn = user_data;
    if (conn->user && conn->engine->cfg.callbacks.stream_acked)
    {
        conn->engine->cfg.callbacks.stream_acked(conn->user, stream_id, datalen);
    }
    return 0;
}

static int cb_stream_close(ngtcp2_conn* qconn, uint32_t flags, int64_t stream_id, uint64_t app_error_code, void* user_data, void* stream_user_data)
{
    (void)qconn;
    (void)flags;
    (void)app_error_code;
    (void)stream_user_data;
    quic_ngtcp2_conn* conn = user_data;
    quic_ngtcp2_stream* st = quic_ngtcp2_stream_get(conn, stream_id);
    if (st)
    {
        st->fin = 1;
        st->write_closed = 1;
        st->engine_closed = 1;
    }
    return 0;
}

static int cb_stream_reset(ngtcp2_conn* qconn, int64_t stream_id, uint64_t final_size, uint64_t app_error_code, void* user_data, void* stream_user_data)
{
    (void)qconn;
    (void)final_size;
    (void)app_error_code;
    (void)stream_user_data;
    quic_ngtcp2_conn* conn = user_data;
    quic_ngtcp2_stream* st = quic_ngtcp2_stream_get(conn, stream_id);
    if (st)
    {
        st->read_reset = 1;
        st->fin = 1;
    }
    return 0;
}

static int cb_extend_max_stream_data(ngtcp2_conn* qconn, int64_t stream_id, uint64_t max_data, void* user_data, void* stream_user_data)
{
    (void)qconn;
    (void)max_data;
    (void)stream_user_data;
    quic_ngtcp2_conn* conn = user_data;
    quic_ngtcp2_stream* st = quic_ngtcp2_stream_get(conn, stream_id);
    if (st)
    {
        st->write_blocked = 0;
    }
    return 0;
}

static void cb_rand(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx* rand_ctx)
{
    (void)rand_ctx;
    RAND_bytes(dest, (int)destlen);
}

static int cb_get_new_connection_id(ngtcp2_conn* qconn, ngtcp2_cid* cid, uint8_t* token, size_t cidlen, void* user_data)
{
    (void)qconn;
    quic_ngtcp2_conn* conn = user_data;
    if (RAND_bytes(cid->data, (int)cidlen) != 1)
    {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    cid->datalen = cidlen;
    if (ngtcp2_crypto_generate_stateless_reset_token(token, conn->engine->secret, sizeof(conn->engine->secret), cid) != 0)
    {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    quic_ngtcp2_cid_add(conn->engine, cid, conn);
    return 0;
}

static int cb_remove_connection_id(ngtcp2_conn* qconn, const ngtcp2_cid* cid, void* user_data)
{
    (void)qconn;
    quic_ngtcp2_conn* conn = user_data;
    quic_ngtcp2_cid_remove(conn->engine, cid);
    return 0;
}

void quic_ngtcp2_callbacks_init(ngtcp2_callbacks* callbacks)
{
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
    callbacks->recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
    callbacks->encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks->decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks->hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks->update_key = ngtcp2_crypto_update_key_cb;
    callbacks->delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks->delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks->get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks->version_negotiation = ngtcp2_crypto_version_negotiation_cb;
    callbacks->handshake_completed = cb_handshake_completed;
    callbacks->recv_stream_data = cb_recv_stream_data;
    callbacks->acked_stream_data_offset = cb_acked_stream_data_offset;
    callbacks->stream_close = cb_stream_close;
    callbacks->stream_reset = cb_stream_reset;
    callbacks->extend_max_stream_data = cb_extend_max_stream_data;
    callbacks->rand = cb_rand;
    callbacks->get_new_connection_id = cb_get_new_connection_id;
    callbacks->remove_connection_id = cb_remove_connection_id;
}

int quic_ngtcp2_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs)
{
    (void)idle_timeout_secs;
    return conn != NULL;
}

void quic_ngtcp2_conn_set_user(quic_conn* conn, void* user)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (nconn)
    {
        nconn->user = user;
    }
}

quic_stream* quic_ngtcp2_conn_open_uni_stream(quic_conn* conn, int64_t* out_id)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    QUIC_CHECK(nconn);
    QUIC_CHECK(out_id);
    if (!nconn->qconn)
    {
        return NULL;
    }
    int64_t stream_id = -1;
    int rv = ngtcp2_conn_open_uni_stream(nconn->qconn, &stream_id, NULL);
    if (rv != 0)
    {
        return NULL;
    }
    *out_id = stream_id;
    return (quic_stream*)quic_ngtcp2_stream_get(nconn, stream_id);
}

quic_stream* quic_ngtcp2_conn_accept_stream(quic_conn* conn)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (!nconn || !nconn->accept_head)
    {
        return NULL;
    }
    quic_ngtcp2_stream* st = nconn->accept_head;
    nconn->accept_head = st->next_accept;
    if (!nconn->accept_head)
    {
        nconn->accept_tail = NULL;
    }
    st->next_accept = NULL;
    st->queued_accept = 0;
    return (quic_stream*)st;
}

int quic_ngtcp2_conn_is_handshake_done(quic_conn* conn)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    return nconn ? (int)nconn->handshake_done : 0;
}

int quic_ngtcp2_conn_is_closed(quic_conn* conn)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (!nconn || nconn->closed)
    {
        return 1;
    }
    return ngtcp2_conn_in_closing_period(nconn->qconn) || ngtcp2_conn_in_draining_period(nconn->qconn);
}

int quic_ngtcp2_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (!nconn)
    {
        return 1;
    }
    if (nconn->closed || !nconn->qconn)
    {
        return 1;
    }
    if (!ngtcp2_conn_in_closing_period(nconn->qconn) && !ngtcp2_conn_in_draining_period(nconn->qconn))
    {
        ngtcp2_ccerr ccerr;
        ngtcp2_ccerr_default(&ccerr);
        if (reason)
        {
            ngtcp2_ccerr_set_application_error(&ccerr, app_error, (const uint8_t*)reason, strlen(reason));
        }
        uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi;
        ngtcp2_ssize n = ngtcp2_conn_write_connection_close(nconn->qconn, &ps.path, &pi, buf, sizeof(buf), &ccerr, quic_ngtcp2_now());
        if (n > 0)
        {
            quic_ngtcp2_send(nconn, &ps.path, buf, (size_t)n);
        }
    }
    (void)is_rapid;
    nconn->closed = 1;
    return 1;
}

void quic_ngtcp2_conn_free(quic_conn* conn)
{
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (!nconn)
    {
        return;
    }
    /* Unlink first, or the engine walks freed memory on the next pump. */
    for (quic_ngtcp2_conn** slot = &nconn->engine->conns_head; *slot; slot = &(*slot)->next)
    {
        if (*slot == nconn)
        {
            *slot = nconn->next;
            break;
        }
    }
    quic_ngtcp2_cid_forget_all(nconn);
    if (nconn->qconn)
    {
        ngtcp2_conn_del(nconn->qconn);
        nconn->qconn = NULL;
    }
    quic_ngtcp2_tls_session_free(nconn);
    while (nconn->streams_head)
    {
        quic_ngtcp2_stream* next = nconn->streams_head->next_stream;
        free(nconn->streams_head->rx_buf);
        free(nconn->streams_head);
        nconn->streams_head = next;
    }
    free(nconn->cids);
    free(nconn);
}
