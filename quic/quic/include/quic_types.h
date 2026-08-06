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

#ifndef QUIC_TYPES_H
#define QUIC_TYPES_H

#include <sys/socket.h>

#include <stddef.h>
#include <stdint.h>

typedef struct quic_engine quic_engine;
typedef struct quic_conn quic_conn;
typedef struct quic_stream quic_stream;

/** Signed byte count, negative on failure, as the io operations return. */
typedef ptrdiff_t quic_ssize;

#define QUIC_ERRLEN 256

typedef struct quic_vec
{
    const uint8_t* base;
    size_t len;
} quic_vec;

/** Datagrams a single batch read may return, and so the array a caller sizes. */
#define QUIC_IO_RECV_BATCH 16

/**
 * One datagram of a batch read. The caller points base at a buffer and sets
 * len to its capacity; the transport overwrites len with the bytes received
 * and fills peer/peer_len with the source address. A len of 0 on return means
 * the datagram did not fit and was dropped, so the caller skips it.
 */
typedef struct quic_dgram
{
    uint8_t* base;
    size_t len;
    struct sockaddr_storage peer;
    socklen_t peer_len;
} quic_dgram;

typedef struct quic_write_result
{
    size_t accepted;
    unsigned blocked : 1;
    unsigned broken : 1;
} quic_write_result;

/*
 * Capability bits are an append-only contract. A bit is never removed, never
 * renumbered and never given a new meaning: callers branch on them, and older
 * callers must keep reading the same answer from newer engines. Add a bit only
 * for a difference a caller must actually adapt to, and record in the new
 * engine's API why it answers differently from the engines already here.
 */
typedef struct quic_caps
{
    unsigned acks_are_write_offsets : 1;
} quic_caps;

/** Where an engine reads its certificate and private key from. */
typedef enum quic_cred_kind
{
    QUIC_CRED_FILE,
    QUIC_CRED_PEM_BUFFER,
} quic_cred_kind;

typedef struct quic_cred
{
    quic_cred_kind kind;
    union
    {
        struct
        {
            const char* cert_path;
            const char* key_path;
        } file;
        struct
        {
            quic_vec cert;
            quic_vec key;
        } pem;
    } as;
} quic_cred;

/** Congestion controller to run, where the engine offers a choice. */
typedef enum quic_cc_algo
{
    QUIC_CC_DEFAULT,
    QUIC_CC_RENO,
    QUIC_CC_CUBIC,
    QUIC_CC_BBR,
} quic_cc_algo;

/**
 * Transport parameters, named as RFC 9000 names them. Fill with
 * quic_settings_default() and overwrite what you mean to change; an engine
 * maps what it can and documents the rest on its API.
 */
typedef struct quic_settings
{
    uint64_t initial_max_data;
    uint64_t initial_max_stream_data_bidi_local;
    uint64_t initial_max_stream_data_bidi_remote;
    uint64_t initial_max_stream_data_uni;
    uint64_t initial_max_streams_bidi;
    uint64_t initial_max_streams_uni;
    uint64_t max_idle_timeout_ms;

    quic_cc_algo cc_algo;
    unsigned enable_datagrams : 1;
    unsigned address_validation : 1;
} quic_settings;

/**
 * Events an engine reports upwards. Every hook takes the handle given to
 * quic_conn_set_user(); leave a hook unset and the engine skips it.
 */
typedef struct quic_callbacks
{
    void (*stream_acked)(void* user, int64_t stream_id, uint64_t datalen);
    void (*handshake_done)(void* user);
    void (*stream_reset)(void* user, int64_t stream_id, uint64_t app_error);
    void (*key_update)(void* user);
    void (*conn_migrated)(void* user, const struct sockaddr* peer, socklen_t peer_len);
} quic_callbacks;

/**
 * How datagrams reach the network, so the engine contract says nothing about
 * sockets. quic_io_udp_init() supplies the ordinary UDP implementation.
 */
typedef struct quic_io
{
    quic_ssize (*send)(void* io_ctx, const uint8_t* buf, size_t len, const struct sockaddr* to, socklen_t to_len);
    quic_ssize (*recv)(void* io_ctx, uint8_t* buf, size_t cap, struct sockaddr_storage* from, socklen_t* from_len);
    int (*local_addr)(void* io_ctx, struct sockaddr_storage* addr, socklen_t* addr_len);
    /* Pollable descriptor, or -1 when this transport has none. */
    int (*fd)(void* io_ctx);
    void* io_ctx;
    /*
     * Optional: read up to ndgrams datagrams in one call, so a busy socket
     * costs one syscall for the batch instead of one per datagram. Returns the
     * number of datagrams read, 0 when none are ready, or -1 on error. Left
     * NULL where the platform or transport has no batch read, and callers then
     * fall back to recv().
     */
    quic_ssize (*recv_batch)(void* io_ctx, quic_dgram* dgrams, size_t ndgrams);
} quic_io;

typedef struct quic_config
{
    quic_cred cred;
    quic_settings settings;
    quic_callbacks callbacks;
    const quic_io* io;
} quic_config;

/** Everything one engine provides, in one table. */
typedef struct quic_api
{
    quic_caps caps;

    struct
    {
        quic_engine* (*create)(const quic_config* cfg, char* err, size_t errlen);
        void (*destroy)(quic_engine* engine);
        int (*pump)(quic_engine* engine);
        void (*want)(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms);
        quic_conn* (*accept_conn)(quic_engine* engine);
        int (*peer_addr)(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len);
        const char* (*last_error)(quic_engine* engine);
    } engine;

    struct
    {
        int (*prepare)(quic_conn* conn, uint32_t idle_timeout_secs);
        void (*set_user)(quic_conn* conn, void* user);
        quic_stream* (*open_uni_stream)(quic_conn* conn, int64_t* out_id);
        quic_stream* (*accept_stream)(quic_conn* conn);
        int (*is_handshake_done)(quic_conn* conn);
        int (*is_closed)(quic_conn* conn);
        int (*shutdown)(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason);
        void (*free)(quic_conn* conn);
    } conn;

    struct
    {
        int64_t (*id)(quic_stream* st);
        quic_write_result (*write)(quic_stream* st, const quic_vec* vec, size_t nvec, int fin);
        int (*is_write_blocked)(quic_stream* st);
        int (*read)(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin);
        void (*is_read_finished)(quic_stream* st, int* read_finished, int* write_finished);
        void (*stop_sending)(quic_stream* st, uint64_t err);
        void (*reset)(quic_stream* st, uint64_t err);
        void (*free)(quic_stream* st);
        void (*consumed)(quic_stream* st, size_t nbytes);
    } stream;
} quic_api;

#endif /* QUIC_TYPES_H */
