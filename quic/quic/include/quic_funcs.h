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

#ifndef QUIC_FUNCS_H
#define QUIC_FUNCS_H

#include "quic_types.h"

/**
 * Make @p name the engine every later call dispatches to. Until this succeeds
 * the first engine compiled in is the one in use.
 * @param name Engine name, matched case-insensitively.
 * @return 1 if this build contains @p name, 0 otherwise, leaving the previous
 *         selection alone.
 */
int quic_select(const char* name);

/**
 * Name of the engine currently selected.
 * @return Engine name; never NULL.
 */
const char* quic_engine_name(void);

/**
 * How many engines this build contains.
 * @return At least one.
 */
size_t quic_engine_count(void);

/**
 * Name of the engine at @p i, for listing what a build offers.
 * @param i Index below quic_engine_count().
 * @return Engine name, or NULL when @p i is out of range.
 */
const char* quic_engine_name_at(size_t i);

/**
 * The selected engine's API.
 * @return Never NULL.
 */
const quic_api* quic_selected(void);

/**
 * Fill @p s with the transport parameters an engine uses when told nothing else.
 * @param s Settings to overwrite.
 */
void quic_settings_default(quic_settings* s);

/**
 * Point @p io at the ordinary UDP implementation over @p fd, which the caller
 * keeps ownership of.
 * @param io Table to fill.
 * @param fd Pre-opened non-blocking UDP socket bound to the listen port.
 */
void quic_io_udp_init(quic_io* io, int fd);

/**
 * Create a QUIC engine over the datagram transport named in @p cfg.
 * @param cfg    Credentials, settings, callbacks and io the engine runs with.
 * @param err    Buffer receiving the reason on failure; may be NULL.
 * @param errlen Capacity of @p err.
 * @return New engine, or NULL on failure.
 */
static inline quic_engine* quic_engine_create(const quic_config* cfg, char* err, size_t errlen)
{
    return quic_selected()->engine.create(cfg, err, errlen);
}

/**
 * Destroy an engine and release its resources.
 * @param engine Engine to destroy; NULL is ignored.
 */
static inline void quic_engine_destroy(quic_engine* engine)
{
    if (engine && quic_selected()->engine.destroy)
    {
        quic_selected()->engine.destroy(engine);
    }
}

/**
 * Drive one round of engine work: read packets, run timers, send.
 * @param engine Engine to pump; NULL reports no work.
 * @return 1 if work was done and another pass may be useful, 0 otherwise.
 */
static inline int quic_engine_pump(quic_engine* engine)
{
    return engine ? quic_selected()->engine.pump(engine) : 0;
}

/**
 * Report what the engine needs from the next event-loop wait.
 * @param engine     Engine to query; NULL leaves the outputs untouched.
 * @param want_read  Out: non-zero if the socket should be polled for reads.
 * @param want_write Out: non-zero if the socket should be polled for writes.
 * @param timeout_ms Out: milliseconds to wait before the next timer is due.
 */
static inline void quic_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms)
{
    if (engine && quic_selected()->engine.want)
    {
        quic_selected()->engine.want(engine, want_read, want_write, timeout_ms);
    }
}

/**
 * Take the next fully handshaken connection.
 * @param engine Engine to accept from; NULL yields NULL.
 * @return Accepted connection, or NULL if none is ready.
 */
static inline quic_conn* quic_engine_accept_conn(quic_engine* engine)
{
    return engine ? quic_selected()->engine.accept_conn(engine) : NULL;
}

/**
 * Resolve a connection's peer address.
 * @param engine   Engine owning @p conn; NULL reports failure.
 * @param conn     Connection to inspect.
 * @param addr     Out: peer socket address.
 * @param addr_len Out: bytes of @p addr that are meaningful.
 * @return 1 if the address was resolved, 0 otherwise.
 */
static inline int quic_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len)
{
    return engine ? quic_selected()->engine.peer_addr(engine, conn, addr, addr_len) : 0;
}

/**
 * The last error the engine recorded, for the caller to log.
 * @param engine Engine to query; NULL reports nothing.
 * @return Message, empty when the engine has reported nothing since the last call.
 */
static inline const char* quic_engine_last_error(quic_engine* engine)
{
    return engine ? quic_selected()->engine.last_error(engine) : "";
}

/**
 * Prepare an accepted connection for use.
 * @param conn              Connection to prepare; NULL reports failure.
 * @param idle_timeout_secs Idle timeout to apply, in seconds.
 * @return 1 on success, 0 on failure.
 */
static inline int quic_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs)
{
    return conn ? quic_selected()->conn.prepare(conn, idle_timeout_secs) : 0;
}

/**
 * Attach the caller's handle to a connection, for callbacks to pass back.
 * @param conn Connection to attach to; NULL is ignored.
 * @param user Caller's handle, or NULL to detach.
 */
static inline void quic_conn_set_user(quic_conn* conn, void* user)
{
    if (conn && quic_selected()->conn.set_user)
    {
        quic_selected()->conn.set_user(conn, user);
    }
}

/**
 * Open a server-initiated unidirectional stream.
 * @param conn   Connection to open on.
 * @param out_id Out: the new stream's id.
 * @return New stream, or NULL on failure.
 */
static inline quic_stream* quic_conn_open_uni_stream(quic_conn* conn, int64_t* out_id)
{
    return quic_selected()->conn.open_uni_stream(conn, out_id);
}

/**
 * Take the next peer-initiated stream.
 * @param conn Connection to accept from.
 * @return Accepted stream, or NULL if none is ready.
 */
static inline quic_stream* quic_conn_accept_stream(quic_conn* conn)
{
    return quic_selected()->conn.accept_stream(conn);
}

/**
 * Whether the TLS handshake has completed.
 * @param conn Connection to query.
 * @return Non-zero once the handshake is done.
 */
static inline int quic_conn_is_handshake_done(quic_conn* conn)
{
    return quic_selected()->conn.is_handshake_done(conn);
}

/**
 * Whether the connection has finished closing.
 * @param conn Connection to query; NULL counts as closed.
 * @return Non-zero once closed.
 */
static inline int quic_conn_is_closed(quic_conn* conn)
{
    return conn ? quic_selected()->conn.is_closed(conn) : 1;
}

/**
 * Begin or continue connection shutdown.
 * @param conn      Connection to close; NULL counts as already closed.
 * @param is_rapid  Non-zero to skip the drain, as on server exit.
 * @param app_error Application error code to report to the peer.
 * @param reason    Text accompanying @p app_error, or NULL to close cleanly.
 * @return 1 when shutdown has completed, 0 while still in progress.
 */
static inline int quic_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason)
{
    return conn ? quic_selected()->conn.shutdown(conn, is_rapid, app_error, reason) : 1;
}

/**
 * Release a connection handle.
 * @param conn Connection to free; NULL is ignored.
 */
static inline void quic_conn_free(quic_conn* conn)
{
    if (conn && quic_selected()->conn.free)
    {
        quic_selected()->conn.free(conn);
    }
}

/**
 * Stream id.
 * @param st Stream to query.
 * @return The stream's id, or -1 if it has none.
 */
static inline int64_t quic_stream_id(quic_stream* st)
{
    return quic_selected()->stream.id(st);
}

/**
 * Write buffers to a stream, optionally closing it.
 * @param st   Stream to write to.
 * @param vec  Buffers to send.
 * @param nvec Number of buffers in @p vec.
 * @param fin  Non-zero to close the stream after these bytes.
 * @return What the engine accepted, and whether it blocked or broke.
 */
static inline quic_write_result quic_stream_write(quic_stream* st, const quic_vec* vec, size_t nvec, int fin)
{
    return quic_selected()->stream.write(st, vec, nvec, fin);
}

/**
 * Whether the stream can currently accept more bytes.
 * @param st Stream to query.
 * @return Non-zero when blocked.
 */
static inline int quic_stream_is_write_blocked(quic_stream* st)
{
    return quic_selected()->stream.is_write_blocked(st);
}

/**
 * Read from a stream.
 * @param st        Stream to read from.
 * @param buf       Destination buffer.
 * @param read_size Capacity of @p buf.
 * @param nread     Out: bytes written to @p buf.
 * @param fin       Out: non-zero once the peer has finished sending.
 * @return 1 if the call succeeded, 0 on failure.
 */
static inline int quic_stream_read(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin)
{
    return quic_selected()->stream.read(st, buf, read_size, nread, fin);
}

/**
 * Report whether each direction of a stream has finished.
 * @param st             Stream to query.
 * @param read_finished  Out: non-zero if reading is finished or reset.
 * @param write_finished Out: non-zero if writing is finished or reset.
 */
static inline void quic_stream_is_read_finished(quic_stream* st, int* read_finished, int* write_finished)
{
    quic_selected()->stream.is_read_finished(st, read_finished, write_finished);
}

/**
 * Ask the peer to stop sending on a stream.
 * @param st  Stream to stop; NULL is ignored.
 * @param err Application error code to report.
 */
static inline void quic_stream_stop_sending(quic_stream* st, uint64_t err)
{
    if (st && quic_selected()->stream.stop_sending)
    {
        quic_selected()->stream.stop_sending(st, err);
    }
}

/**
 * Abort the sending half of a stream.
 * @param st  Stream to reset; NULL is ignored.
 * @param err Application error code to report.
 */
static inline void quic_stream_reset(quic_stream* st, uint64_t err)
{
    if (st && quic_selected()->stream.reset)
    {
        quic_selected()->stream.reset(st, err);
    }
}

/**
 * Free a stream handle.
 * @param st Stream to free; NULL is ignored.
 */
static inline void quic_stream_free(quic_stream* st)
{
    if (st && quic_selected()->stream.free)
    {
        quic_selected()->stream.free(st);
    }
}

/**
 * Credit stream flow control for bytes the application consumed.
 * @param st     Stream that was read from.
 * @param nbytes Bytes consumed.
 */
static inline void quic_stream_consumed(quic_stream* st, size_t nbytes)
{
    if (quic_selected()->stream.consumed)
    {
        quic_selected()->stream.consumed(st, nbytes);
    }
}

#endif /* QUIC_FUNCS_H */
