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

#ifndef QUIC_NGTCP2_IMPL_H
#define QUIC_NGTCP2_IMPL_H

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#include <openssl/ssl.h>

#include "detail/quic_check.h"
#include "quic_ngtcp2.h"

typedef struct quic_ngtcp2_conn quic_ngtcp2_conn;
typedef struct quic_ngtcp2_stream quic_ngtcp2_stream;

typedef struct quic_ngtcp2_map_slot
{
    uint8_t key[NGTCP2_MAX_CIDLEN];
    size_t keylen;
    quic_ngtcp2_conn* conn;
    /* Stays set after deletion, so probe chains through this slot survive. */
    unsigned used : 1;
} quic_ngtcp2_map_slot;

typedef struct quic_ngtcp2_map
{
    quic_ngtcp2_map_slot* slots;
    size_t cap;
    size_t len;
} quic_ngtcp2_map;

/**
 * Publish @p conn under the connection ID in @p key, growing the table as needed.
 * @param map    Table to insert into.
 * @param key    Connection ID bytes.
 * @param keylen Length of @p key, at most NGTCP2_MAX_CIDLEN.
 * @param conn   Connection that answers to @p key.
 * @return 1 on success, 0 if the table could not grow.
 */
int quic_ngtcp2_map_set(quic_ngtcp2_map* map, const uint8_t* key, size_t keylen, quic_ngtcp2_conn* conn);

/**
 * Look up the connection published under a connection ID.
 * @param map    Table to search.
 * @param key    Connection ID bytes.
 * @param keylen Length of @p key.
 * @return The connection, or NULL if @p key is not published.
 */
quic_ngtcp2_conn* quic_ngtcp2_map_get(const quic_ngtcp2_map* map, const uint8_t* key, size_t keylen);

/**
 * Retract a connection ID.
 * @param map    Table to remove from.
 * @param key    Connection ID bytes.
 * @param keylen Length of @p key.
 */
void quic_ngtcp2_map_del(quic_ngtcp2_map* map, const uint8_t* key, size_t keylen);

/**
 * Release the table's storage.
 * @param map Table to free; its slots are not owned by the entries.
 */
void quic_ngtcp2_map_free(quic_ngtcp2_map* map);

#define QUIC_NGTCP2_SCIDLEN 18
#define QUIC_NGTCP2_MAX_UDP_PAYLOAD 1452
#define QUIC_NGTCP2_RECV_BUDGET 64

/*
 * Per-datagram buffer of a batch read. QUIC peers keep datagrams inside the
 * path MTU, so this is generous for a 1200-byte max_udp_payload_size while
 * keeping a whole batch on the stack. Anything larger is dropped by the batch
 * read rather than parsed half-complete.
 */
#define QUIC_NGTCP2_DGRAM_MAX 2048
#define QUIC_NGTCP2_RETRY_TOKEN_TIMEOUT (10 * NGTCP2_SECONDS)

struct quic_engine
{
    quic_config cfg;

    SSL_CTX* ssl_ctx;
    const quic_io* io;
    int validate_addr;
    uint64_t idle_timeout_ns;
    uint8_t secret[32];
    quic_ngtcp2_map conns;
    quic_ngtcp2_conn* conns_head;
    quic_ngtcp2_conn* accept_head;
    quic_ngtcp2_conn* accept_tail;
    /* Set while err holds a message the caller has not collected. */
    int err_pending;
    char err[QUIC_ERRLEN];
};

struct quic_ngtcp2_conn
{
    quic_engine* engine;
    ngtcp2_conn* qconn;
    ngtcp2_crypto_conn_ref conn_ref;
    /* ngtcp2 takes this, not the SSL*, as the native TLS handle. */
    ngtcp2_crypto_ossl_ctx* ossl_ctx;
    SSL* ssl;
    ngtcp2_cid scid;
    ngtcp2_path_storage path;
    quic_ngtcp2_stream* streams_head;
    /* Every CID published, so teardown retracts exactly those. */
    ngtcp2_cid* cids;
    size_t cids_len;
    size_t cids_cap;
    quic_ngtcp2_stream* accept_head;
    quic_ngtcp2_stream* accept_tail;
    void* user;
    quic_ngtcp2_conn* next;
    quic_ngtcp2_conn* next_accept;
    unsigned handshake_done : 1;
    unsigned closed : 1;
    unsigned queued_accept : 1;
};

struct quic_ngtcp2_stream
{
    quic_ngtcp2_conn* conn;
    int64_t stream_id;

    unsigned char* rx_buf;
    size_t rx_len;
    size_t rx_cap;
    size_t rx_off;

    quic_ngtcp2_stream* next_accept;
    quic_ngtcp2_stream* next_stream;

    unsigned fin : 1;
    unsigned write_blocked : 1;
    unsigned read_reset : 1;
    unsigned write_closed : 1;
    unsigned queued_accept : 1;
    unsigned engine_closed : 1;
    /* TI: FIN accepted from nghttp3 but refused by ngtcp2; only we can still retry it. */
    unsigned fin_pending : 1;
};

/**
 * Monotonic clock in the nanosecond units ngtcp2 requires.
 * @return Current time, suitable for every ngtcp2 timestamp parameter.
 */
ngtcp2_tstamp quic_ngtcp2_now(void);

/**
 * Build the TLS context, adding the ngtcp2 crypto helper's own initialisation
 * to what the shared TLS layer sets up.
 * @param cfg    Configuration supplying the certificate and key paths.
 * @param err    Buffer receiving the reason on failure; may be NULL.
 * @param errlen Capacity of @p err.
 * @return New context, or NULL on failure.
 */
SSL_CTX* quic_ngtcp2_tls_ctx_create(const quic_config* cfg, char* err, size_t errlen);

/**
 * Bind an OpenSSL session to @p conn using ngtcp2's ossl crypto helper.
 * @param conn Connection whose ngtcp2_conn has already been created.
 * @return 1 on success, 0 on failure.
 */
int quic_ngtcp2_tls_session_init(quic_ngtcp2_conn* conn);

/**
 * Release the TLS session.
 * @param conn Connection to tear down. The app data is detached before the
 *             SSL is freed, which ngtcp2's teardown ordering requires.
 */
void quic_ngtcp2_tls_session_free(quic_ngtcp2_conn* conn);

/**
 * Populate @p callbacks with the server callback set.
 * @param callbacks Out: every callback ngtcp2 requires of a server.
 */
void quic_ngtcp2_callbacks_init(ngtcp2_callbacks* callbacks);

/**
 * Associate @p cid with @p conn in the engine's routing table.
 * @param engine Engine holding the table.
 * @param cid    Connection ID to publish.
 * @param conn   Connection that answers to @p cid.
 */
void quic_ngtcp2_cid_add(quic_engine* engine, const ngtcp2_cid* cid, quic_ngtcp2_conn* conn);

/**
 * Retract every connection ID @p conn published.
 * @param conn Connection being torn down.
 */
void quic_ngtcp2_cid_forget_all(quic_ngtcp2_conn* conn);

/**
 * Drop @p cid from the engine's routing table.
 * @param engine Engine holding the table.
 * @param cid    Connection ID to retract.
 */
void quic_ngtcp2_cid_remove(quic_engine* engine, const ngtcp2_cid* cid);

/**
 * Look up the connection owning @p cid.
 * @param engine Engine holding the table.
 * @param cid    Destination connection ID from an inbound packet.
 * @return Owning connection, or NULL if no connection answers to @p cid.
 */
quic_ngtcp2_conn* quic_ngtcp2_cid_find(quic_engine* engine, const ngtcp2_cid* cid);

/**
 * Queue @p conn for delivery through the engine's accept_conn op.
 * @param conn Connection whose handshake has completed.
 */
void quic_ngtcp2_queue_accept(quic_ngtcp2_conn* conn);

/**
 * Write any packets ngtcp2 has pending for @p conn to the wire.
 * Also retries a FIN ngtcp2 previously refused, and ends by updating the packet
 * transmit time, without which ngtcp2 never paces.
 * @param conn Connection to flush; NULL and closed connections are ignored.
 */
void quic_ngtcp2_conn_flush(quic_ngtcp2_conn* conn);

/**
 * Send one datagram along @p path from the engine's socket.
 * @param conn Connection the datagram belongs to.
 * @param path Network path ngtcp2 chose for it.
 * @param buf  Datagram payload.
 * @param len  Length of @p buf.
 */
void quic_ngtcp2_send(quic_ngtcp2_conn* conn, const ngtcp2_path* path, const uint8_t* buf, size_t len);

/**
 * Find or create the stream record for @p stream_id.
 * @param conn      Connection owning the stream.
 * @param stream_id QUIC stream id.
 * @return The stream record, or NULL if @p conn is NULL.
 */
quic_ngtcp2_stream* quic_ngtcp2_stream_get(quic_ngtcp2_conn* conn, int64_t stream_id);

/**
 * Append received bytes and queue the stream for acceptance.
 * @param conn      Connection the data arrived on.
 * @param stream_id Stream the data belongs to.
 * @param data      Received bytes; copied into the stream's buffer.
 * @param datalen   Length of @p data.
 * @param fin       Non-zero if the peer finished sending.
 * @return 1 when the data was taken, 0 if the stream could not be resolved.
 */
int quic_ngtcp2_stream_recv(quic_ngtcp2_conn* conn, int64_t stream_id, const uint8_t* data, size_t datalen, int fin);

#endif /* QUIC_NGTCP2_IMPL_H */
