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

#ifndef QUIC_OSSL_H
#define QUIC_OSSL_H

#include "detail/quic_ossl_funcs.h"

/**
 * API for the OpenSSL QUIC engine. Sets
 * caps.acks_are_write_offsets: OpenSSL reports no per-stream acknowledgements,
 * so bytes count as acked once SSL_write_ex takes them. Entries left unset are the
 * ones this engine does not need; stop_sending among them, since OpenSSL closes
 * the receiving half as part of the stream's own teardown.
 * Leaves caps.early_data clear: OpenSSL's own QUIC stack does not accept 0-RTT
 * on the server side, so settings.early_data cannot take effect here.
 * @note Of quic_settings it honours max_idle_timeout_ms, address_validation and
 *       session_tickets. OpenSSL fixes its own flow-control windows and
 *       congestion control, so the initial_max_* fields, cc_algo and
 *       enable_datagrams are ignored.
 * @return Table with static storage duration; never NULL.
 */
static inline const quic_api* quic_ossl_api(void)
{
    static const quic_api api = {
        .caps =
            {
                .acks_are_write_offsets = 1,
            },
        .engine =
            {
                .create = quic_ossl_engine_create,
                .destroy = quic_ossl_engine_destroy,
                .pump = quic_ossl_engine_pump,
                .want = quic_ossl_engine_want,
                .accept_conn = quic_ossl_engine_accept_conn,
                .peer_addr = quic_ossl_engine_peer_addr,
                .last_error = quic_ossl_engine_last_error,
            },
        .conn =
            {
                .prepare = quic_ossl_conn_prepare,
                .set_user = NULL,
                .open_uni_stream = quic_ossl_conn_open_uni_stream,
                .accept_stream = quic_ossl_conn_accept_stream,
                .is_handshake_done = quic_ossl_conn_is_handshake_done,
                .is_closed = quic_ossl_conn_is_closed,
                .shutdown = quic_ossl_conn_shutdown,
                .free = quic_ossl_conn_free,
            },
        .stream =
            {
                .id = quic_ossl_stream_id,
                .write = quic_ossl_stream_write,
                .is_write_blocked = quic_ossl_stream_is_write_blocked,
                .read = quic_ossl_stream_read,
                .is_read_finished = quic_ossl_stream_is_read_finished,
                .stop_sending = NULL,
                .reset = quic_ossl_stream_reset,
                .free = quic_ossl_stream_free,
                .consumed = NULL,
            },
    };
    return &api;
}

#endif /* QUIC_OSSL_H */
