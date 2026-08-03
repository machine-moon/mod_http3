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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "detail/quic_ngtcp2_impl.h"

/* quic_vec and ngtcp2_vec are both {uint8_t *base; size_t len}. */
static_assert(sizeof(quic_vec) == sizeof(ngtcp2_vec), "vec size mismatch");
static_assert(offsetof(quic_vec, base) == offsetof(ngtcp2_vec, base), "vec base offset mismatch");
static_assert(offsetof(quic_vec, len) == offsetof(ngtcp2_vec, len), "vec len offset mismatch");

quic_ngtcp2_stream* quic_ngtcp2_stream_get(quic_ngtcp2_conn* conn, int64_t stream_id)
{
    if (!conn)
    {
        return NULL;
    }
    quic_ngtcp2_stream* st = conn->qconn ? ngtcp2_conn_get_stream_user_data(conn->qconn, stream_id) : NULL;
    if (st)
    {
        return st;
    }
    st = calloc(1, sizeof(*st));
    if (!st)
    {
        return NULL;
    }
    st->conn = conn;
    st->stream_id = stream_id;
    st->next_stream = conn->streams_head;
    conn->streams_head = st;
    if (conn->qconn)
    {
        ngtcp2_conn_set_stream_user_data(conn->qconn, stream_id, st);
    }
    return st;
}

int quic_ngtcp2_stream_recv(quic_ngtcp2_conn* conn, int64_t stream_id, const uint8_t* data, size_t datalen, int fin)
{
    quic_ngtcp2_stream* st = quic_ngtcp2_stream_get(conn, stream_id);
    if (!st)
    {
        return 0;
    }
    if (datalen > 0)
    {
        size_t needed = st->rx_len + datalen;
        if (needed > st->rx_cap)
        {
            size_t cap = st->rx_cap ? st->rx_cap : 4096;
            while (cap < needed)
            {
                cap *= 2;
            }
            unsigned char* grown = realloc(st->rx_buf, cap);
            if (!grown)
            {
                return 0;
            }
            st->rx_buf = grown;
            st->rx_cap = cap;
        }
        memcpy(st->rx_buf + st->rx_len, data, datalen);
        st->rx_len += datalen;
    }
    if (fin)
    {
        st->fin = 1;
    }
    if (!st->queued_accept)
    {
        st->queued_accept = 1;
        if (conn->accept_tail)
        {
            conn->accept_tail->next_accept = st;
        }
        else
        {
            conn->accept_head = st;
        }
        conn->accept_tail = st;
    }
    return 1;
}

int64_t quic_ngtcp2_stream_id(quic_stream* st)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    return nst ? nst->stream_id : -1;
}

quic_write_result quic_ngtcp2_stream_write(quic_stream* st, const quic_vec* vec, size_t nvec, int fin)
{
    quic_write_result res = {0};
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst || !nst->conn || !nst->conn->qconn || nst->write_closed)
    {
        res.broken = 1;
        return res;
    }

    quic_ngtcp2_conn* conn = nst->conn;
    const ngtcp2_vec* datav = (const ngtcp2_vec*)vec;
    size_t vec_idx = 0;
    size_t vec_off = 0;
    uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];

    while (vec_idx < nvec || fin)
    {
        ngtcp2_vec head;
        const ngtcp2_vec* send_vec = NULL;
        size_t send_cnt = 0;
        if (vec_idx < nvec)
        {
            head.base = datav[vec_idx].base + vec_off;
            head.len = datav[vec_idx].len - vec_off;
            send_vec = &head;
            send_cnt = 1;
        }

        int last = (vec_idx + 1 >= nvec) && (send_cnt == 0 || head.len == datav[vec_idx].len - vec_off);
        uint32_t flags = (fin && last) ? NGTCP2_WRITE_STREAM_FLAG_FIN : NGTCP2_WRITE_STREAM_FLAG_NONE;

        ngtcp2_ssize ndatalen = 0;
        ngtcp2_pkt_info pi;
        /* ngtcp2 fills ps with its own storage, so it must not alias conn->path. */
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_ssize n = ngtcp2_conn_writev_stream(conn->qconn, &ps.path, &pi, buf, sizeof(buf), &ndatalen, flags, nst->stream_id, send_vec, send_cnt, quic_ngtcp2_now());

        if (n < 0)
        {
            if (n == NGTCP2_ERR_STREAM_DATA_BLOCKED)
            {
                nst->write_blocked = 1;
                res.blocked = 1;
            }
            else
            {
                /* Everything else, SHUT_WR included, is terminal: no window update will clear it. */
                nst->write_closed = 1;
                res.broken = 1;
            }
            break;
        }

        if (n > 0)
        {
            quic_ngtcp2_send(conn, &ps.path, buf, (size_t)n);
        }

        if (ndatalen > 0)
        {
            res.accepted += (size_t)ndatalen;
            size_t remaining = (size_t)ndatalen;
            while (remaining > 0 && vec_idx < nvec)
            {
                size_t chunk = datav[vec_idx].len - vec_off;
                if (chunk > remaining)
                {
                    vec_off += remaining;
                    remaining = 0;
                }
                else
                {
                    remaining -= chunk;
                    vec_idx++;
                    vec_off = 0;
                }
            }
        }

        if (n == 0)
        {
            // TI: an owed FIN counts as blocked, or the stream never completes
            if (vec_idx < nvec || (fin && !nst->write_closed))
            {
                res.blocked = 1;
            }
            if (vec_idx >= nvec && fin && !nst->write_closed)
            {
                nst->fin_pending = 1;
            }
            break;
        }

        if (vec_idx >= nvec && (flags & NGTCP2_WRITE_STREAM_FLAG_FIN))
        {
            /* ndatalen stays -1 when other frames crowded the STREAM frame out, so the FIN never went. */
            if (ndatalen < 0)
            {
                res.blocked = 1;
                nst->fin_pending = 1;
                break;
            }
            nst->fin_pending = 0;
            nst->write_closed = 1;
            break;
        }
        if (vec_idx >= nvec && !fin)
        {
            break;
        }
    }
    /* conn_flush ends with ngtcp2_conn_update_pkt_tx_time, required after any write round. */
    quic_ngtcp2_conn_flush(conn);
    return res;
}

int quic_ngtcp2_stream_is_write_blocked(quic_stream* st)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst || !nst->conn || !nst->conn->qconn || nst->write_closed)
    {
        return 1;
    }
    unsigned long cdl = (unsigned long)ngtcp2_conn_get_max_data_left(nst->conn->qconn);
    unsigned long sdl = (unsigned long)ngtcp2_conn_get_max_stream_data_left(nst->conn->qconn, nst->stream_id);
    if (cdl == 0)
    {
        return 1;
    }
    return sdl == 0;
}

int quic_ngtcp2_stream_read(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    *nread = 0;
    *fin = 0;
    if (!nst)
    {
        return 0;
    }
    size_t avail = nst->rx_len - nst->rx_off;
    if (avail == 0)
    {
        if (nst->fin)
        {
            *fin = 1;
        }
        return 0;
    }
    size_t copied = read_size < avail ? read_size : avail;
    memcpy(buf, nst->rx_buf + nst->rx_off, copied);
    nst->rx_off += copied;
    *nread = copied;
    if (nst->rx_off == nst->rx_len)
    {
        /* Fully drained: reuse the allocation instead of growing it per body. */
        nst->rx_off = 0;
        nst->rx_len = 0;
    }
    if (nst->rx_off >= nst->rx_len && nst->fin)
    {
        *fin = 1;
    }
    return 1;
}

void quic_ngtcp2_stream_is_read_finished(quic_stream* st, int* read_finished, int* write_finished)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst)
    {
        *read_finished = 1;
        *write_finished = 1;
        return;
    }
    *read_finished = (nst->read_reset || (nst->fin && nst->rx_off >= nst->rx_len)) ? 1 : 0;
    /* Not write_closed: ngtcp2 retransmits from our buffers until it closes the stream. */
    *write_finished = (nst->engine_closed || !nst->conn || !nst->conn->qconn) ? 1 : 0;
}

void quic_ngtcp2_stream_consumed(quic_stream* st, size_t nbytes)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst || !nst->conn || !nst->conn->qconn || nbytes == 0)
    {
        return;
    }
    ngtcp2_conn_extend_max_stream_offset(nst->conn->qconn, nst->stream_id, nbytes);
    ngtcp2_conn_extend_max_offset(nst->conn->qconn, nbytes);
    /* The peer is window-blocked until MAX_STREAM_DATA reaches it. */
    quic_ngtcp2_conn_flush(nst->conn);
}

void quic_ngtcp2_stream_stop_sending(quic_stream* st, uint64_t err)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst)
    {
        return;
    }
    if (nst->conn && nst->conn->qconn)
    {
        ngtcp2_conn_shutdown_stream_read(nst->conn->qconn, 0, nst->stream_id, err);
    }
    nst->read_reset = 1;
}

void quic_ngtcp2_stream_reset(quic_stream* st, uint64_t err)
{
    quic_ngtcp2_stream* nst = (quic_ngtcp2_stream*)st;
    if (!nst)
    {
        return;
    }
    if (nst->conn && nst->conn->qconn)
    {
        ngtcp2_conn_shutdown_stream_write(nst->conn->qconn, 0, nst->stream_id, err);
    }
    nst->write_closed = 1;
}

/* Owned by the connection, so teardown runs from quic_ngtcp2_conn_free(). */
void quic_ngtcp2_stream_free(quic_stream* st)
{
    (void)st;
}
