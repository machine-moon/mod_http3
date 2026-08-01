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

#define QUIC_ERRLEN 256

typedef struct quic_vec
{
    const uint8_t* base;
    size_t len;
} quic_vec;

typedef struct quic_write_result
{
    size_t accepted;
    unsigned blocked : 1;
    unsigned broken : 1;
} quic_write_result;

typedef struct quic_caps
{
    unsigned acks_are_write_offsets : 1;
} quic_caps;

typedef struct quic_config
{
    const char* cert_path;
    const char* key_path;
    unsigned address_validation : 1;
    uint32_t idle_timeout_secs;
    void (*on_stream_acked)(void* user, int64_t stream_id, uint64_t datalen);
} quic_config;

typedef struct quic_engine_ops
{
    quic_engine* (*create)(const quic_config* cfg, int udp_fd, char* err, size_t errlen);
    void (*destroy)(quic_engine* engine);
    void (*socket_configure)(quic_engine* engine, int fd);
    int (*pump)(quic_engine* engine);
    void (*want)(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms);
    quic_conn* (*accept_conn)(quic_engine* engine);
    int (*peer_addr)(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len);
    const char* (*last_error)(quic_engine* engine);
} quic_engine_ops;

typedef struct quic_conn_ops
{
    int (*prepare)(quic_conn* conn, uint32_t idle_timeout_secs);
    void (*set_user)(quic_conn* conn, void* user);
    quic_stream* (*open_uni_stream)(quic_conn* conn, int64_t* out_id);
    quic_stream* (*accept_stream)(quic_conn* conn);
    int (*is_handshake_done)(quic_conn* conn);
    int (*is_closed)(quic_conn* conn);
    int (*shutdown)(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason);
    void (*free)(quic_conn* conn);
} quic_conn_ops;

typedef struct quic_stream_ops
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
} quic_stream_ops;

typedef struct quic_ops
{
    quic_caps caps;
    quic_engine_ops engine;
    quic_conn_ops conn;
    quic_stream_ops stream;
} quic_ops;

#endif /* QUIC_TYPES_H */
