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

#ifndef QUIC_TLS_H
#define QUIC_TLS_H

#include <openssl/ssl.h>

#include "quic_types.h"

/**
 * Build the TLS context both engines serve from: certificate and key from
 * @p cfg, "h3" as the only ALPN protocol, and a key log when SSLKEYLOGFILE is
 * set. The method decides who owns the QUIC framing, so the OpenSSL engine
 * passes OSSL_QUIC_server_method() and ngtcp2 passes TLS_server_method().
 * @param method TLS method the context is created with.
 * @param cfg    Configuration supplying the certificate and key paths.
 * @param err    Buffer receiving the reason on failure; may be NULL.
 * @param errlen Capacity of @p err.
 * @return New context, or NULL on failure.
 */
SSL_CTX* quic_tls_ctx_create(const SSL_METHOD* method, const quic_config* cfg, char* err, size_t errlen);

/**
 * Record a message in a caller-supplied error buffer, appending the OpenSSL
 * error queue's own text when it has any.
 * @param err    Buffer to write to; NULL is ignored.
 * @param errlen Capacity of @p err.
 * @param fmt    printf-style format for the message.
 */
void quic_tls_error(char* err, size_t errlen, const char* fmt, ...);

#endif /* QUIC_TLS_H */
