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

#ifndef QUIC_NGTCP2_H
#define QUIC_NGTCP2_H

#include "detail/quic_ngtcp2_funcs.h"

/**
 * API for the ngtcp2 QUIC engine, built only under ENABLE_NGTCP2.
 * Clears caps.acks_are_write_offsets: ngtcp2 reports real per-stream
 * acknowledgements and retransmits from the caller's buffers, so they must be
 * held until it acknowledges them.
 * @note Honours every quic_settings field: the initial_max_* windows and
 *       address_validation become transport parameters, max_idle_timeout_ms
 *       and cc_algo reach ngtcp2 directly, and enable_datagrams advertises
 *       max_datagram_frame_size.
 * @return Table with static storage duration; never NULL.
 */
static inline const quic_api* quic_ngtcp2_api(void)
{
    static const quic_api api = {
        .caps =
            {
                .acks_are_write_offsets = 0,
            },
        .engine =
            {
                .create = quic_ngtcp2_engine_create,
                .destroy = quic_ngtcp2_engine_destroy,
                .pump = quic_ngtcp2_engine_pump,
                .want = quic_ngtcp2_engine_want,
                .accept_conn = quic_ngtcp2_engine_accept_conn,
                .peer_addr = quic_ngtcp2_engine_peer_addr,
                .last_error = quic_ngtcp2_engine_last_error,
            },
        .conn =
            {
                .prepare = quic_ngtcp2_conn_prepare,
                .set_user = quic_ngtcp2_conn_set_user,
                .open_uni_stream = quic_ngtcp2_conn_open_uni_stream,
                .accept_stream = quic_ngtcp2_conn_accept_stream,
                .is_handshake_done = quic_ngtcp2_conn_is_handshake_done,
                .is_closed = quic_ngtcp2_conn_is_closed,
                .shutdown = quic_ngtcp2_conn_shutdown,
                .free = quic_ngtcp2_conn_free,
            },
        .stream =
            {
                .id = quic_ngtcp2_stream_id,
                .write = quic_ngtcp2_stream_write,
                .is_write_blocked = quic_ngtcp2_stream_is_write_blocked,
                .read = quic_ngtcp2_stream_read,
                .is_read_finished = quic_ngtcp2_stream_is_read_finished,
                .stop_sending = quic_ngtcp2_stream_stop_sending,
                .reset = quic_ngtcp2_stream_reset,
                .free = quic_ngtcp2_stream_free,
                .consumed = quic_ngtcp2_stream_consumed,
            },
    };
    return &api;
}

#endif /* QUIC_NGTCP2_H */
