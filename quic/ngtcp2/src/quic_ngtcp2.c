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

#include <sys/socket.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <openssl/rand.h>

#include "detail/quic_check.h"
#include "detail/quic_ngtcp2_impl.h"
#include "detail/quic_tls.h"
#include "quic.h"
#include "quic_ngtcp2.h"

ngtcp2_tstamp quic_ngtcp2_now(void)
{
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return (ngtcp2_tstamp)tp.tv_sec * NGTCP2_SECONDS + (ngtcp2_tstamp)tp.tv_nsec;
}

void quic_ngtcp2_send(quic_ngtcp2_conn* conn, const ngtcp2_path* path, const uint8_t* buf, size_t len)
{
    const struct sockaddr* dst = (const struct sockaddr*)path->remote.addr;
    while (conn->engine->io->send(conn->engine->io->io_ctx, buf, len, dst, (socklen_t)path->remote.addrlen) < 0 && errno == EINTR)
    {
    }
}

/* TI: nghttp3 offers the FIN once, so one ngtcp2 refuses is ours alone to retry. */
static void retry_pending_fins(quic_ngtcp2_conn* conn)
{
    uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
    for (quic_ngtcp2_stream* nst = conn->streams_head; nst; nst = nst->next_stream)
    {
        if (!nst || !nst->fin_pending || nst->write_closed || nst->engine_closed)
        {
            continue;
        }
        ngtcp2_ssize ndatalen = 0;
        ngtcp2_pkt_info pi;
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_ssize n = ngtcp2_conn_writev_stream(conn->qconn, &ps.path, &pi, buf, sizeof(buf), &ndatalen, NGTCP2_WRITE_STREAM_FLAG_FIN, nst->stream_id, NULL, 0, quic_ngtcp2_now());
        if (n < 0)
        {
            if (n != NGTCP2_ERR_STREAM_DATA_BLOCKED)
            {
                nst->fin_pending = 0;
                nst->write_closed = 1;
            }
            continue;
        }
        if (n == 0)
        {
            continue; /* still congestion limited; a later expiry retries */
        }
        quic_ngtcp2_send(conn, &ps.path, buf, (size_t)n);
        if (ndatalen >= 0)
        {
            nst->fin_pending = 0;
            nst->write_closed = 1;
        }
    }
}

void quic_ngtcp2_conn_flush(quic_ngtcp2_conn* conn)
{
    if (!conn || !conn->qconn || conn->closed)
    {
        return;
    }
    retry_pending_fins(conn);
    uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
    for (;;)
    {
        /* ngtcp2 fills ps with its own storage, so it must not alias conn->path. */
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi;
        ngtcp2_ssize n = ngtcp2_conn_write_pkt(conn->qconn, &ps.path, &pi, buf, sizeof(buf), quic_ngtcp2_now());
        if (n <= 0)
        {
            if (n < 0)
            {
                conn->closed = 1;
            }
            return;
        }
        quic_ngtcp2_send(conn, &ps.path, buf, (size_t)n);
    }
    /* Required after writing; without it ngtcp2 never paces and bursts the whole window. */
    ngtcp2_conn_update_pkt_tx_time(conn->qconn, quic_ngtcp2_now());
}

void quic_ngtcp2_queue_accept(quic_ngtcp2_conn* conn)
{
    quic_engine* engine = conn->engine;
    if (conn->queued_accept)
    {
        return;
    }
    conn->queued_accept = 1;
    if (engine->accept_tail)
    {
        engine->accept_tail->next_accept = conn;
    }
    else
    {
        engine->accept_head = conn;
    }
    engine->accept_tail = conn;
}

static void engine_send_raw(quic_engine* engine, const struct sockaddr* dst, socklen_t dstlen, const uint8_t* buf, size_t len)
{
    while (engine->io->send(engine->io->io_ctx, buf, len, dst, dstlen) < 0 && errno == EINTR)
    {
    }
}

/* A peer probing with an unknown version must be told what we do speak. */
static void send_version_negotiation(quic_engine* engine, const ngtcp2_version_cid* vc, const struct sockaddr* peer, socklen_t peerlen)
{
    static const uint32_t versions[] = {NGTCP2_PROTO_VER_V1, NGTCP2_PROTO_VER_V2};
    uint8_t unused_random = 0;
    if (RAND_bytes(&unused_random, 1) != 1)
    {
        return;
    }
    uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
    /* The reply swaps the connection IDs: their source becomes our destination. */
    ngtcp2_ssize n = ngtcp2_pkt_write_version_negotiation(buf, sizeof(buf), unused_random, vc->scid, vc->scidlen, vc->dcid, vc->dcidlen, versions, sizeof(versions) / sizeof(versions[0]));
    if (n > 0)
    {
        engine_send_raw(engine, peer, peerlen, buf, (size_t)n);
    }
}

static void send_retry(quic_engine* engine, const ngtcp2_pkt_hd* hd, const struct sockaddr* peer, socklen_t peerlen)
{
    ngtcp2_cid scid;
    scid.datalen = QUIC_NGTCP2_SCIDLEN;
    if (RAND_bytes(scid.data, (int)scid.datalen) != 1)
    {
        return;
    }

    uint8_t token[NGTCP2_CRYPTO_MAX_RETRY_TOKENLEN2];
    ngtcp2_ssize tokenlen = ngtcp2_crypto_generate_retry_token2(token, engine->secret, sizeof(engine->secret), hd->version, (const ngtcp2_sockaddr*)peer, (ngtcp2_socklen)peerlen, &scid, &hd->dcid, quic_ngtcp2_now());
    if (tokenlen < 0)
    {
        return;
    }

    uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
    ngtcp2_ssize n = ngtcp2_crypto_write_retry(buf, sizeof(buf), hd->version, &hd->scid, &scid, &hd->dcid, token, (size_t)tokenlen);
    if (n > 0)
    {
        engine_send_raw(engine, peer, peerlen, buf, (size_t)n);
    }
}

/* Tell the peer why we are closing, rather than leaving it to time out. */
static void conn_close_with(quic_ngtcp2_conn* conn, uint64_t code, int is_tls_alert)
{
    if (conn->qconn && !ngtcp2_conn_in_closing_period(conn->qconn) && !ngtcp2_conn_in_draining_period(conn->qconn))
    {
        ngtcp2_ccerr ccerr;
        ngtcp2_ccerr_default(&ccerr);
        if (is_tls_alert)
        {
            ngtcp2_ccerr_set_tls_alert(&ccerr, (uint8_t)code, NULL, 0);
        }
        else
        {
            ngtcp2_ccerr_set_liberr(&ccerr, (int)code, NULL, 0);
        }
        uint8_t buf[QUIC_NGTCP2_MAX_UDP_PAYLOAD];
        ngtcp2_path_storage ps;
        ngtcp2_path_storage_zero(&ps);
        ngtcp2_pkt_info pi;
        ngtcp2_ssize n = ngtcp2_conn_write_connection_close(conn->qconn, &ps.path, &pi, buf, sizeof(buf), &ccerr, quic_ngtcp2_now());
        if (n > 0)
        {
            quic_ngtcp2_send(conn, &ps.path, buf, (size_t)n);
        }
    }
    conn->closed = 1;
}

/* ngtcp2 asks for a Retry when it cannot accept the Initial as it stands. */
static void send_retry_for(quic_engine* engine, const uint8_t* pkt, size_t pktlen, const struct sockaddr* peer, socklen_t peerlen)
{
    ngtcp2_pkt_hd hd;
    if (ngtcp2_accept(&hd, pkt, pktlen) == 0)
    {
        send_retry(engine, &hd, peer, peerlen);
    }
}

static quic_ngtcp2_conn* conn_new(quic_engine* engine, const ngtcp2_pkt_hd* hd, const ngtcp2_cid* odcid, const ngtcp2_cid* retry_scid, const struct sockaddr* peer, socklen_t peerlen, const struct sockaddr* local, socklen_t locallen)
{
    quic_ngtcp2_conn* conn = calloc(1, sizeof(*conn));
    if (!conn)
    {
        return NULL;
    }
    conn->engine = engine;

    conn->scid.datalen = QUIC_NGTCP2_SCIDLEN;
    if (RAND_bytes(conn->scid.data, (int)conn->scid.datalen) != 1)
    {
        quic_ngtcp2_conn_free((quic_conn*)conn);
        return NULL;
    }

    ngtcp2_path_storage_init(&conn->path, (const ngtcp2_sockaddr*)local, (ngtcp2_socklen)locallen, (const ngtcp2_sockaddr*)peer, (ngtcp2_socklen)peerlen, NULL);

    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = quic_ngtcp2_now();
    switch (engine->cfg.settings.cc_algo)
    {
        case QUIC_CC_RENO: settings.cc_algo = NGTCP2_CC_ALGO_RENO; break;
        case QUIC_CC_CUBIC: settings.cc_algo = NGTCP2_CC_ALGO_CUBIC; break;
        case QUIC_CC_BBR: settings.cc_algo = NGTCP2_CC_ALGO_BBR; break;
        case QUIC_CC_DEFAULT: break;
    }

    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    const quic_settings* set = &engine->cfg.settings;
    params.max_idle_timeout = engine->idle_timeout_ns;
    params.initial_max_data = set->initial_max_data;
    params.initial_max_stream_data_bidi_local = set->initial_max_stream_data_bidi_local;
    params.initial_max_stream_data_bidi_remote = set->initial_max_stream_data_bidi_remote;
    params.initial_max_stream_data_uni = set->initial_max_stream_data_uni;
    params.initial_max_streams_bidi = set->initial_max_streams_bidi;
    params.initial_max_streams_uni = set->initial_max_streams_uni;
    if (set->enable_datagrams)
    {
        params.max_datagram_frame_size = QUIC_NGTCP2_MAX_UDP_PAYLOAD;
    }
    params.original_dcid = odcid ? *odcid : hd->dcid;
    params.original_dcid_present = 1;
    if (retry_scid)
    {
        params.retry_scid = *retry_scid;
        params.retry_scid_present = 1;
    }
    if (ngtcp2_crypto_generate_stateless_reset_token(params.stateless_reset_token, engine->secret, sizeof(engine->secret), &conn->scid) == 0)
    {
        params.stateless_reset_token_present = 1;
    }

    ngtcp2_callbacks callbacks;
    quic_ngtcp2_callbacks_init(&callbacks);

    int rv = ngtcp2_conn_server_new(&conn->qconn, &hd->scid, &conn->scid, &conn->path.path, hd->version, &callbacks, &settings, &params, NULL, conn);
    if (rv != 0)
    {
        snprintf(engine->err, sizeof(engine->err), "ngtcp2_conn_server_new failed: %s", ngtcp2_strerror(rv));
        engine->err_pending = 1;
        quic_ngtcp2_conn_free((quic_conn*)conn);
        return NULL;
    }

    if (!quic_ngtcp2_tls_session_init(conn))
    {
        snprintf(engine->err, sizeof(engine->err), "binding an OpenSSL session to the ngtcp2 connection failed");
        engine->err_pending = 1;
        ngtcp2_conn_del(conn->qconn);
        quic_ngtcp2_conn_free((quic_conn*)conn);
        return NULL;
    }

    quic_ngtcp2_cid_add(engine, &conn->scid, conn);
    /* A retransmitted Initial carries the original DCID; without it the map forks a connection. */
    quic_ngtcp2_cid_add(engine, &hd->dcid, conn);
    size_t nscid = ngtcp2_conn_get_scid(conn->qconn, NULL);
    if (nscid > 0)
    {
        ngtcp2_cid* scids = calloc(nscid, sizeof(*scids));
        if (scids)
        {
            ngtcp2_conn_get_scid(conn->qconn, scids);
            for (size_t i = 0; i < nscid; i++)
            {
                quic_ngtcp2_cid_add(engine, &scids[i], conn);
            }
            free(scids);
        }
    }
    conn->next = engine->conns_head;
    engine->conns_head = conn;
    quic_ngtcp2_queue_accept(conn);
    return conn;
}

static quic_ngtcp2_conn* conn_accept(quic_engine* engine, const uint8_t* pkt, size_t pktlen, const struct sockaddr* peer, socklen_t peerlen, const struct sockaddr* local, socklen_t locallen)
{
    ngtcp2_pkt_hd hd;
    if (ngtcp2_accept(&hd, pkt, pktlen) != 0)
    {
        return NULL;
    }

    if (!engine->validate_addr)
    {
        return conn_new(engine, &hd, NULL, NULL, peer, peerlen, local, locallen);
    }

    if (hd.tokenlen == 0 || hd.token[0] != NGTCP2_CRYPTO_TOKEN_MAGIC_RETRY2)
    {
        send_retry(engine, &hd, peer, peerlen);
        return NULL;
    }

    ngtcp2_cid odcid;
    if (ngtcp2_crypto_verify_retry_token2(&odcid, hd.token, hd.tokenlen, engine->secret, sizeof(engine->secret), hd.version, (const ngtcp2_sockaddr*)peer, (ngtcp2_socklen)peerlen, &hd.dcid, QUIC_NGTCP2_RETRY_TOKEN_TIMEOUT, quic_ngtcp2_now()) != 0)
    {
        send_retry(engine, &hd, peer, peerlen);
        return NULL;
    }
    return conn_new(engine, &hd, &odcid, &hd.dcid, peer, peerlen, local, locallen);
}

static void engine_expire(quic_engine* engine)
{
    ngtcp2_tstamp now = quic_ngtcp2_now();
    for (quic_ngtcp2_conn* conn = engine->conns_head; conn; conn = conn->next)
    {
        if (conn->closed || !conn->qconn)
        {
            continue;
        }
        if (ngtcp2_conn_get_expiry2(conn->qconn) > now)
        {
            continue;
        }
        if (ngtcp2_conn_handle_expiry(conn->qconn, now) != 0)
        {
            conn->closed = 1;
            continue;
        }
        quic_ngtcp2_conn_flush(conn);
    }
}

quic_engine* quic_ngtcp2_engine_create(const quic_config* cfg, char* err, size_t errlen)
{
    QUIC_CHECK(cfg);
    QUIC_CHECK(cfg->io);

    quic_engine* engine = calloc(1, sizeof(*engine));
    if (!engine)
    {
        quic_tls_error(err, errlen, "allocating the engine failed");
        return NULL;
    }
    engine->cfg = *cfg;
    engine->io = cfg->io;
    engine->validate_addr = cfg->settings.address_validation;
    engine->idle_timeout_ns = cfg->settings.max_idle_timeout_ms * NGTCP2_MILLISECONDS;

    if (RAND_bytes(engine->secret, (int)sizeof(engine->secret)) != 1)
    {
        quic_tls_error(err, errlen, "RAND_bytes failed while seeding the token secret");
        quic_ngtcp2_engine_destroy(engine);
        return NULL;
    }

    engine->ssl_ctx = quic_ngtcp2_tls_ctx_create(cfg, err, errlen);
    if (!engine->ssl_ctx)
    {
        quic_ngtcp2_engine_destroy(engine);
        return NULL;
    }
    return engine;
}

void quic_ngtcp2_engine_destroy(quic_engine* engine)
{
    if (!engine)
    {
        return;
    }
    while (engine->conns_head)
    {
        quic_ngtcp2_conn* next = engine->conns_head->next;
        quic_ngtcp2_conn_free((quic_conn*)engine->conns_head);
        engine->conns_head = next;
    }
    quic_ngtcp2_map_free(&engine->conns);
    if (engine->ssl_ctx)
    {
        SSL_CTX_free(engine->ssl_ctx);
    }
    free(engine);
}

const char* quic_ngtcp2_engine_last_error(quic_engine* engine)
{
    if (!engine || !engine->err_pending)
    {
        return "";
    }
    engine->err_pending = 0;
    return engine->err;
}

static void process_dgram(quic_engine* engine, uint8_t* buf, size_t nread, struct sockaddr_storage* peer, socklen_t peerlen, struct sockaddr_storage* local, socklen_t locallen)
{
    ngtcp2_version_cid vc;
    int rv = ngtcp2_pkt_decode_version_cid(&vc, buf, nread, QUIC_NGTCP2_SCIDLEN);
    if (rv != 0)
    {
        if (rv == NGTCP2_ERR_VERSION_NEGOTIATION)
        {
            send_version_negotiation(engine, &vc, (struct sockaddr*)peer, peerlen);
        }
        return;
    }

    ngtcp2_cid dcid;
    ngtcp2_cid_init(&dcid, vc.dcid, vc.dcidlen);
    quic_ngtcp2_conn* conn = quic_ngtcp2_cid_find(engine, &dcid);
    if (!conn)
    {
        conn = conn_accept(engine, buf, nread, (struct sockaddr*)peer, peerlen, (struct sockaddr*)local, locallen);
        if (!conn)
        {
            return;
        }
    }
    if (conn->closed || !conn->qconn)
    {
        return;
    }

    ngtcp2_path path = {
        .local = {.addr = (ngtcp2_sockaddr*)local, .addrlen = (ngtcp2_socklen)locallen},
        .remote = {.addr = (ngtcp2_sockaddr*)peer, .addrlen = (ngtcp2_socklen)peerlen},
    };
    ngtcp2_pkt_info pi = {0};
    rv = ngtcp2_conn_read_pkt(conn->qconn, &path, &pi, buf, nread, quic_ngtcp2_now());
    if (rv != 0)
    {
        switch (rv)
        {
        case NGTCP2_ERR_RETRY:
            /* A stateless Retry is owed; the connection is not at fault. */
            send_retry_for(engine, buf, nread, (struct sockaddr*)peer, peerlen);
            return;
        case NGTCP2_ERR_DROP_CONN:
            conn->closed = 1;
            return;
        case NGTCP2_ERR_DRAINING:
        case NGTCP2_ERR_CLOSING:
            conn->closed = 1;
            return;
        case NGTCP2_ERR_CRYPTO:
            conn_close_with(conn, ngtcp2_conn_get_tls_alert(conn->qconn), 1);
            return;
        default:
            snprintf(engine->err, sizeof(engine->err), "ngtcp2_conn_read_pkt: %s", ngtcp2_strerror(rv));
            engine->err_pending = 1;
            conn_close_with(conn, (uint64_t)rv, 0);
            return;
        }
    }
    quic_ngtcp2_conn_flush(conn);
}

/* One recvmmsg() per batch instead of one recvfrom() per datagram. */
static int pump_batched(quic_engine* engine, struct sockaddr_storage* local, socklen_t locallen)
{
    uint8_t bufs[QUIC_IO_RECV_BATCH][QUIC_NGTCP2_DGRAM_MAX];
    quic_dgram dgrams[QUIC_IO_RECV_BATCH];
    int progressed = 0;

    for (int taken = 0; taken < QUIC_NGTCP2_RECV_BUDGET; taken += QUIC_IO_RECV_BATCH)
    {
        for (size_t i = 0; i < QUIC_IO_RECV_BATCH; i++)
        {
            dgrams[i].base = bufs[i];
            dgrams[i].len = sizeof(bufs[i]);
        }
        quic_ssize n = engine->io->recv_batch(engine->io->io_ctx, dgrams, QUIC_IO_RECV_BATCH);
        if (n <= 0)
        {
            break;
        }
        progressed = 1;
        for (quic_ssize i = 0; i < n; i++)
        {
            if (dgrams[i].len > 0)
            {
                process_dgram(engine, dgrams[i].base, dgrams[i].len, &dgrams[i].peer, dgrams[i].peer_len, local, locallen);
            }
        }
        if ((size_t)n < QUIC_IO_RECV_BATCH)
        {
            /* A short batch means the socket is drained. */
            break;
        }
    }
    return progressed;
}

static int pump_one_by_one(quic_engine* engine, struct sockaddr_storage* local, socklen_t locallen)
{
    uint8_t buf[65536];
    int progressed = 0;

    for (int i = 0; i < QUIC_NGTCP2_RECV_BUDGET; i++)
    {
        struct sockaddr_storage peer;
        socklen_t peerlen = sizeof(peer);
        ssize_t nread;
        do
        {
            nread = engine->io->recv(engine->io->io_ctx, buf, sizeof(buf), &peer, &peerlen);
        } while (nread < 0 && errno == EINTR);
        if (nread < 0)
        {
            break;
        }
        progressed = 1;
        process_dgram(engine, buf, (size_t)nread, &peer, peerlen, local, locallen);
    }
    return progressed;
}

int quic_ngtcp2_engine_pump(quic_engine* engine)
{
    if (!engine || !engine->io)
    {
        return 0;
    }

    /* The socket stays bound for the engine's lifetime, so ngtcp2 sees no path
     * change and the local address is worth reading once per pump rather than
     * once per datagram. */
    struct sockaddr_storage local;
    socklen_t locallen = sizeof(local);
    if (!engine->io->local_addr(engine->io->io_ctx, &local, &locallen))
    {
        return 0;
    }

    int progressed = engine->io->recv_batch ? pump_batched(engine, &local, locallen) : pump_one_by_one(engine, &local, locallen);

    engine_expire(engine);
    return progressed;
}

void quic_ngtcp2_engine_want(quic_engine* engine, int* want_read, int* want_write, int* timeout_ms)
{
    *want_read = 1;
    *want_write = 0;
    *timeout_ms = 1000;

    ngtcp2_tstamp now = quic_ngtcp2_now();
    ngtcp2_tstamp earliest = UINT64_MAX;
    for (quic_ngtcp2_conn* conn = engine->conns_head; conn; conn = conn->next)
    {
        if (conn->closed || !conn->qconn)
        {
            continue;
        }
        ngtcp2_tstamp expiry = ngtcp2_conn_get_expiry2(conn->qconn);
        if (expiry < earliest)
        {
            earliest = expiry;
        }
    }
    if (earliest == UINT64_MAX)
    {
        return;
    }
    if (earliest <= now)
    {
        *timeout_ms = 0;
        return;
    }
    uint64_t delta_ms = (earliest - now) / NGTCP2_MILLISECONDS;
    if (delta_ms < (uint64_t)*timeout_ms)
    {
        *timeout_ms = (int)delta_ms;
    }
}

quic_conn* quic_ngtcp2_engine_accept_conn(quic_engine* engine)
{
    if (!engine || !engine->accept_head)
    {
        return NULL;
    }
    quic_ngtcp2_conn* conn = engine->accept_head;
    engine->accept_head = conn->next_accept;
    if (!engine->accept_head)
    {
        engine->accept_tail = NULL;
    }
    conn->next_accept = NULL;
    conn->queued_accept = 0;
    return (quic_conn*)conn;
}

int quic_ngtcp2_engine_peer_addr(quic_engine* engine, quic_conn* conn, struct sockaddr_storage* addr, socklen_t* addr_len)
{
    (void)engine;
    quic_ngtcp2_conn* nconn = (quic_ngtcp2_conn*)conn;
    if (!nconn || !addr || !addr_len)
    {
        return 0;
    }

    socklen_t len = (socklen_t)nconn->path.path.remote.addrlen;
    if (len == 0 || len > (socklen_t)sizeof(*addr))
    {
        return 0;
    }
    memcpy(addr, nconn->path.path.remote.addr, len);
    *addr_len = len;
    return 1;
}
