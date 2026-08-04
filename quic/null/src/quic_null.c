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

#include "quic.h"
#include "quic_null.h"

struct quic_engine
{
    quic_config cfg;
};

quic_engine* quic_null_engine_create(const quic_config* cfg, char* err, size_t errlen)
{
    (void)err;
    (void)errlen;
    quic_engine* engine = calloc(1, sizeof(*engine));
    if (engine && cfg)
    {
        engine->cfg = *cfg;
    }
    return engine;
}

void quic_null_engine_destroy(quic_engine* engine)
{
    free(engine);
}

int quic_null_engine_pump(quic_engine* engine)
{
    (void)engine;
    return 0;
}

void quic_null_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms)
{
    (void)engine;
    *want_read = 0;
    *want_write = 0;
    *timeout_ms = -1;
}

quic_conn* quic_null_engine_accept_conn(quic_engine* engine)
{
    (void)engine;
    return NULL;
}

int quic_null_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len)
{
    (void)engine;
    (void)conn;
    (void)addr;
    (void)addr_len;
    return 0;
}

const char* quic_null_engine_last_error(quic_engine* engine)
{
    (void)engine;
    return "";
}

int quic_null_conn_prepare(quic_conn* conn, uint32_t idle_timeout_secs)
{
    (void)conn;
    (void)idle_timeout_secs;
    return 0;
}

quic_stream* quic_null_conn_open_uni_stream(quic_conn* conn, int64_t* out_id)
{
    (void)conn;
    (void)out_id;
    return NULL;
}

quic_stream* quic_null_conn_accept_stream(quic_conn* conn)
{
    (void)conn;
    return NULL;
}

int quic_null_conn_is_handshake_done(quic_conn* conn)
{
    (void)conn;
    return 0;
}

int quic_null_conn_is_closed(quic_conn* conn)
{
    (void)conn;
    return 1;
}

int quic_null_conn_shutdown(quic_conn* conn, int is_rapid, uint64_t app_error, const char* reason)
{
    (void)conn;
    (void)is_rapid;
    (void)app_error;
    (void)reason;
    return 1;
}

int64_t quic_null_stream_id(quic_stream* st)
{
    (void)st;
    return -1;
}

quic_write_result quic_null_stream_write(quic_stream* st, const quic_vec* vec, size_t nvec, int fin)
{
    (void)st;
    (void)vec;
    (void)nvec;
    (void)fin;
    return (quic_write_result){.broken = 1};
}

int quic_null_stream_is_write_blocked(quic_stream* st)
{
    (void)st;
    return 0;
}

int quic_null_stream_read(quic_stream* st, unsigned char* buf, size_t read_size, size_t* nread, int* fin)
{
    (void)st;
    (void)buf;
    (void)read_size;
    *nread = 0;
    *fin = 1;
    return 0;
}

void quic_null_stream_is_read_finished(quic_stream* st, int* read_finished, int* write_finished)
{
    (void)st;
    *read_finished = 1;
    *write_finished = 1;
}
