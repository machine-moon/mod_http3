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

#ifndef QUIC_NGTCP2_FUNCS_H
#define QUIC_NGTCP2_FUNCS_H

#include "quic_types.h"

/**
 * Open the TLS context and the connection-ID routing table over @p udp_fd.
 * Unlike OpenSSL's QUIC, ngtcp2 owns no listener: the engine reads datagrams
 * itself and routes each one by destination connection ID.
 * @param cfg    Credentials, settings, callbacks and io the engine runs with.
 * @param err    Buffer receiving the reason on failure; may be NULL.
 * @param errlen Capacity of @p err.
 * @return New engine, or NULL on failure.
 */
quic_engine* quic_ngtcp2_engine_create(const quic_config* cfg, char* err, size_t errlen);

/**
 * Close every live connection and release the TLS context.
 * @param engine Engine to destroy.
 */
void quic_ngtcp2_engine_destroy(quic_engine* engine);

/**
 * Read a budget of datagrams, run expiry timers, then flush what is pending.
 * @param engine Engine to pump.
 * @return 1 if work was done and another pass may be useful, 0 otherwise.
 */
int quic_ngtcp2_engine_pump(quic_engine* engine);

/**
 * Report what the engine needs from the next event-loop wait.
 * @param engine     Engine to query.
 * @param want_read  Out: non-zero if the socket should be polled for reads.
 * @param want_write Out: non-zero if the socket should be polled for writes.
 * @param timeout_ms Out: milliseconds until the earliest ngtcp2 timer is due.
 */
void quic_ngtcp2_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms);

/**
 * Take the next handshaken connection off the accept queue.
 * @param engine Engine to accept from.
 * @return Accepted connection, or NULL if none is ready.
 */
quic_conn* quic_ngtcp2_engine_accept_conn(quic_engine* engine);

/**
 * Report the remote address of the connection's current network path.
 * @param engine   Engine owning @p conn.
 * @param conn     Connection to inspect.
 * @param addr     Out: peer socket address.
 * @param addr_len Out: bytes of @p addr that are meaningful.
 * @return 1 if the address was reported, 0 otherwise.
 */
int quic_ngtcp2_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len);

/**
 * The last error the engine recorded, chiefly from the packet-read path.
 * @param engine Engine to query.
 * @return Message, empty when nothing new has been recorded since the last call.
 */
const char* quic_ngtcp2_engine_last_error(quic_engine* engine);

/**
 * Apply the idle timeout to a freshly accepted connection.
 * @param conn              Connection to prepare.
 * @param idle_timeout_secs Idle timeout to apply, in seconds.
 * @return 1 on success, 0 on failure.
 */
int quic_ngtcp2_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs);

/**
 * Attach the caller's handle, which the acknowledgement callback passes back.
 * @param conn Connection to attach to.
 * @param user Caller's handle, or NULL to detach.
 */
void quic_ngtcp2_conn_set_user(quic_conn* conn, void* user);

/**
 * Open a server-initiated unidirectional stream.
 * @param conn   Connection to open on.
 * @param out_id Out: the new stream's id.
 * @return New stream, or NULL on failure.
 */
quic_stream* quic_ngtcp2_conn_open_uni_stream(quic_conn* conn, int64_t* out_id);

/**
 * Take the next peer-initiated stream.
 * @param conn Connection to accept from.
 * @return Accepted stream, or NULL if none is ready.
 */
quic_stream* quic_ngtcp2_conn_accept_stream(quic_conn* conn);

/**
 * Whether the TLS handshake has completed.
 * @param conn Connection to query.
 * @return Non-zero once the handshake is done.
 */
int quic_ngtcp2_conn_is_handshake_done(quic_conn* conn);

/**
 * Whether the connection has finished closing.
 * @param conn Connection to query.
 * @return Non-zero once closed.
 */
int quic_ngtcp2_conn_is_closed(quic_conn* conn);

/**
 * Send CONNECTION_CLOSE and finish the connection.
 * @param conn      Connection to close.
 * @param is_rapid  Non-zero to skip the drain, as on server exit.
 * @param app_error Application error code to report to the peer.
 * @param reason    Text accompanying @p app_error, or NULL to close cleanly.
 * @return 1 when shutdown has completed, 0 while still in progress.
 */
int quic_ngtcp2_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason);

/**
 * Release a connection handle and retract every connection ID it published.
 * @param conn Connection to free.
 */
void quic_ngtcp2_conn_free(quic_conn* conn);

/**
 * Stream id.
 * @param st Stream to query.
 * @return The stream's id, or -1 when @p st is NULL.
 */
int64_t quic_ngtcp2_stream_id(quic_stream* st);

/**
 * Write buffers to a stream, optionally closing it. ngtcp2 retransmits from
 * these buffers, so they must stay valid until it acknowledges them.
 * @param st   Stream to write to.
 * @param vec  Buffers to send.
 * @param nvec Number of buffers in @p vec.
 * @param fin  Non-zero to close the stream after these bytes.
 * @return What the engine accepted, and whether it blocked or broke.
 */
quic_write_result quic_ngtcp2_stream_write(quic_stream* st, const quic_vec* vec, size_t nvec, int fin);

/**
 * Whether connection or stream flow control currently leaves no room.
 * @param st Stream to query.
 * @return Non-zero when blocked.
 */
int quic_ngtcp2_stream_is_write_blocked(quic_stream* st);

/**
 * Read from a stream's receive buffer.
 * @param st        Stream to read from.
 * @param buf       Destination buffer.
 * @param read_size Capacity of @p buf.
 * @param nread     Out: bytes written to @p buf.
 * @param fin       Out: non-zero once the peer has finished sending.
 * @return 1 if the call succeeded, 0 on failure.
 */
int quic_ngtcp2_stream_read(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin);

/**
 * Report whether each direction of a stream has finished.
 * @param st             Stream to query.
 * @param read_finished  Out: non-zero if reading is finished or reset.
 * @param write_finished Out: non-zero if writing is finished or reset.
 */
void quic_ngtcp2_stream_is_read_finished(quic_stream* st, int* read_finished, int* write_finished);

/**
 * Ask the peer to stop sending on a stream.
 * @param st  Stream to stop.
 * @param err Application error code to report.
 */
void quic_ngtcp2_stream_stop_sending(quic_stream* st, uint64_t err);

/**
 * Abort the sending half of a stream.
 * @param st  Stream to reset.
 * @param err Application error code to report.
 */
void quic_ngtcp2_stream_reset(quic_stream* st, uint64_t err);

/**
 * Free a stream handle. The connection owns its streams, so this is a no-op
 * and teardown happens in quic_ngtcp2_conn_free().
 * @param st Stream to free.
 */
void quic_ngtcp2_stream_free(quic_stream* st);

/**
 * Credit stream flow control for bytes the application consumed.
 * @param st     Stream that was read from.
 * @param nbytes Bytes consumed.
 */
void quic_ngtcp2_stream_consumed(quic_stream* st, size_t nbytes);

#endif /* QUIC_NGTCP2_FUNCS_H */
