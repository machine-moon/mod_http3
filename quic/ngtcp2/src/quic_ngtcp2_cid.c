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

#include "detail/quic_ngtcp2_impl.h"

void quic_ngtcp2_cid_add(quic_engine* engine, const ngtcp2_cid* cid, quic_ngtcp2_conn* conn)
{
    if (!engine || !cid || !conn || cid->datalen == 0)
    {
        return;
    }
    if (!quic_ngtcp2_map_set(&engine->conns, cid->data, cid->datalen, conn))
    {
        return;
    }
    if (conn->cids_len == conn->cids_cap)
    {
        size_t cap = conn->cids_cap ? conn->cids_cap * 2 : 8;
        ngtcp2_cid* grown = realloc(conn->cids, cap * sizeof(*grown));
        if (!grown)
        {
            quic_ngtcp2_map_del(&engine->conns, cid->data, cid->datalen);
            return;
        }
        conn->cids = grown;
        conn->cids_cap = cap;
    }
    /* ngtcp2 may reject a CID after its callback returns, so track what we published. */
    conn->cids[conn->cids_len++] = *cid;
}

void quic_ngtcp2_cid_forget_all(quic_ngtcp2_conn* conn)
{
    if (!conn || !conn->cids)
    {
        return;
    }
    for (size_t i = 0; i < conn->cids_len; i++)
    {
        quic_ngtcp2_cid_remove(conn->engine, &conn->cids[i]);
    }
    conn->cids_len = 0;
}

void quic_ngtcp2_cid_remove(quic_engine* engine, const ngtcp2_cid* cid)
{
    if (!engine || !cid || cid->datalen == 0)
    {
        return;
    }
    quic_ngtcp2_map_del(&engine->conns, cid->data, cid->datalen);
}

quic_ngtcp2_conn* quic_ngtcp2_cid_find(quic_engine* engine, const ngtcp2_cid* cid)
{
    if (!engine || !cid || cid->datalen == 0)
    {
        return NULL;
    }
    return quic_ngtcp2_map_get(&engine->conns, cid->data, cid->datalen);
}
