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

#ifndef QUIC_NULL_FUNCS_H
#define QUIC_NULL_FUNCS_H

#include "quic_types.h"

/**
 * Accept the configuration and stand up an engine that never carries traffic.
 * @param cfg    Configuration, which this engine only validates.
 * @param err    Buffer receiving the reason on failure; may be NULL.
 * @param errlen Capacity of @p err.
 * @return New engine, or NULL on allocation failure.
 */
quic_engine* quic_null_engine_create(const quic_config* cfg, char* err, size_t errlen);

/**
 * Release the engine.
 * @param engine Engine to destroy.
 */
void quic_null_engine_destroy(quic_engine* engine);

/**
 * Report that there is never any work to do.
 * @param engine Engine to pump.
 * @return Always 0.
 */
int quic_null_engine_pump(quic_engine* engine);

/**
 * Ask the event loop to sleep rather than poll.
 * @param engine     Engine to query.
 * @param want_read  Out: always 0.
 * @param want_write Out: always 0.
 * @param timeout_ms Out: always -1.
 */
void quic_null_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms);

/**
 * Never produce a connection.
 * @param engine Engine to accept from.
 * @return Always NULL.
 */
quic_conn* quic_null_engine_accept_conn(quic_engine* engine);

/**
 * Report that no peer address is known.
 * @param engine   Engine owning @p conn.
 * @param conn     Connection to inspect.
 * @param addr     Out: untouched.
 * @param addr_len Out: untouched.
 * @return Always 0.
 */
int quic_null_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len);

/**
 * The last error the engine recorded.
 * @param engine Engine to query.
 * @return Always the empty string.
 */
const char* quic_null_engine_last_error(quic_engine* engine);

/**
 * Fail to prepare a connection this engine can never have produced.
 * @param conn              Connection to prepare.
 * @param idle_timeout_secs Idle timeout to apply, in seconds.
 * @return Always 0.
 */
int quic_null_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs);

/**
 * Fail to open a stream.
 * @param conn   Connection to open on.
 * @param out_id Out: untouched.
 * @return Always NULL.
 */
quic_stream* quic_null_conn_open_uni_stream(quic_conn* conn, int64_t* out_id);

/**
 * Never produce a stream.
 * @param conn Connection to accept from.
 * @return Always NULL.
 */
quic_stream* quic_null_conn_accept_stream(quic_conn* conn);

/**
 * Report that no handshake ever completes.
 * @param conn Connection to query.
 * @return Always 0.
 */
int quic_null_conn_is_handshake_done(quic_conn* conn);

/**
 * Report the connection as closed.
 * @param conn Connection to query.
 * @return Always 1.
 */
int quic_null_conn_is_closed(quic_conn* conn);

/**
 * Report shutdown as already complete.
 * @param conn      Connection to close.
 * @param is_rapid  Non-zero to skip the drain.
 * @param app_error Application error code to report to the peer.
 * @param reason    Text accompanying @p app_error, or NULL.
 * @return Always 1.
 */
int quic_null_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason);

/**
 * Stream id.
 * @param st Stream to query.
 * @return Always -1.
 */
int64_t quic_null_stream_id(quic_stream* st);

/**
 * Discard a write, reporting the stream as broken.
 * @param st   Stream to write to.
 * @param vec  Buffers to send.
 * @param nvec Number of buffers in @p vec.
 * @param fin  Non-zero to close the stream after these bytes.
 * @return Nothing accepted, broken set.
 */
quic_write_result quic_null_stream_write(quic_stream* st, const quic_vec* vec, size_t nvec, int fin);

/**
 * Report the stream as never write-blocked.
 * @param st Stream to query.
 * @return Always 0.
 */
int quic_null_stream_is_write_blocked(quic_stream* st);

/**
 * Fail every read.
 * @param st        Stream to read from.
 * @param buf       Destination buffer.
 * @param read_size Capacity of @p buf.
 * @param nread     Out: zero.
 * @param fin       Out: non-zero.
 * @return Always 0.
 */
int quic_null_stream_read(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin);

/**
 * Report both directions as finished.
 * @param st             Stream to query.
 * @param read_finished  Out: always 1.
 * @param write_finished Out: always 1.
 */
void quic_null_stream_is_read_finished(quic_stream* st, int* read_finished, int* write_finished);

#endif /* QUIC_NULL_FUNCS_H */
