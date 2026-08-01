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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "detail/quic_check.h"
#include "detail/quic_tls.h"

void quic_tls_error(char* err, size_t errlen, const char* fmt, ...)
{
    if (!err || errlen == 0)
    {
        ERR_clear_error();
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(err, errlen, fmt, ap);
    va_end(ap);

    unsigned long code = ERR_get_error();
    if (code != 0 && n > 0 && (size_t)n + 2 < errlen)
    {
        char detail[QUIC_ERRLEN] = {0};
        ERR_error_string_n(code, detail, sizeof(detail));
        snprintf(err + n, errlen - (size_t)n, ": %s", detail);
    }
    ERR_clear_error();
}

static int quic_tls_alpn_select_cb(SSL* ssl, const unsigned char** out, unsigned char* outlen, const unsigned char* in, unsigned int inlen, void* arg)
{
    static const unsigned char h3[] = "\x02h3";
    (void)ssl;
    (void)arg;

    if (SSL_select_next_proto((unsigned char**)out, outlen, h3, sizeof(h3) - 1, in, inlen) == OPENSSL_NPN_NEGOTIATED)
    {
        return SSL_TLSEXT_ERR_OK;
    }
    return SSL_TLSEXT_ERR_NOACK;
}

static void quic_tls_keylog_cb(const SSL* ssl, const char* line)
{
    (void)ssl;
    const char* path = getenv("SSLKEYLOGFILE");
    FILE* f = path ? fopen(path, "a") : NULL;
    if (f)
    {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

SSL_CTX* quic_tls_ctx_create(const SSL_METHOD* method, const quic_config* cfg, char* err, size_t errlen)
{
    QUIC_CHECK(method);
    QUIC_CHECK(cfg);

    SSL_CTX* ssl_ctx = SSL_CTX_new(method);
    if (!ssl_ctx)
    {
        quic_tls_error(err, errlen, "SSL_CTX_new failed");
        return NULL;
    }

    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_3_VERSION);

    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cfg->cert_path) <= 0 || SSL_CTX_use_PrivateKey_file(ssl_ctx, cfg->key_path, SSL_FILETYPE_PEM) <= 0)
    {
        quic_tls_error(err, errlen, "loading the certificate or private key failed");
        SSL_CTX_free(ssl_ctx);
        return NULL;
    }

    SSL_CTX_set_alpn_select_cb(ssl_ctx, quic_tls_alpn_select_cb, NULL);
    if (getenv("SSLKEYLOGFILE"))
    {
        SSL_CTX_set_keylog_callback(ssl_ctx, quic_tls_keylog_cb);
    }
    return ssl_ctx;
}
