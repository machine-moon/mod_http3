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

#ifndef QUIC_NULL_H
#define QUIC_NULL_H

#include "detail/quic_null_funcs.h"

/**
 * API for the engine that carries nothing. It exists so the
 * contract stays addable-to: wiring a backend in must touch its own directory
 * and one registry row, and nothing else. Selecting it leaves the server
 * listening but never completing a handshake, which also makes it a way to run
 * the module with no transport underneath.
 * Clears caps.acks_are_write_offsets, since it reports no acknowledgements of
 * any kind and no caller should synthesise them from writes that never happen.
 * @note Accepts every quic_settings field and honours none: nothing is sent.
 * @return Table with static storage duration; never NULL.
 */
static inline const quic_api* quic_null_api(void)
{
    static const quic_api api = {
        .caps =
            {
                .acks_are_write_offsets = 0,
            },
        .engine =
            {
                .create = quic_null_engine_create,
                .destroy = quic_null_engine_destroy,
                .pump = quic_null_engine_pump,
                .want = quic_null_engine_want,
                .accept_conn = quic_null_engine_accept_conn,
                .peer_addr = quic_null_engine_peer_addr,
                .last_error = quic_null_engine_last_error,
            },
        .conn =
            {
                .prepare = quic_null_conn_prepare,
                .set_user = NULL,
                .open_uni_stream = quic_null_conn_open_uni_stream,
                .accept_stream = quic_null_conn_accept_stream,
                .is_handshake_done = quic_null_conn_is_handshake_done,
                .is_closed = quic_null_conn_is_closed,
                .shutdown = quic_null_conn_shutdown,
                .free = NULL,
            },
        .stream =
            {
                .id = quic_null_stream_id,
                .write = quic_null_stream_write,
                .is_write_blocked = quic_null_stream_is_write_blocked,
                .read = quic_null_stream_read,
                .is_read_finished = quic_null_stream_is_read_finished,
                .stop_sending = NULL,
                .reset = NULL,
                .free = NULL,
                .consumed = NULL,
            },
    };
    return &api;
}

#endif /* QUIC_NULL_H */
