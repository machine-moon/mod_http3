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

#ifndef H3_RESPONSE_COMPAT_H
#define H3_RESPONSE_COMPAT_H

#include <httpd.h>

#include "h3_compat.h"
#include "h3_filter.h"

#if !H3_HAS_RESPONSE_BUCKETS

/**
 * Finalize the response of @p r the way the core HTTP_HEADER filter would
 * (merge err_headers_out, materialize Content-Type/-Encoding/-Language,
 * dedup Vary, add Date/Server, honor no-cache and header-only statuses) and
 * snapshot the resulting status and headers into @p h3ctx. On servers with
 * response buckets this arrives as an ap_bucket_response instead; on 2.4.x
 * the HTTP_HEADER filter is removed from H3 requests and this port supplies
 * the same data. Idempotent: does nothing once h3ctx->resp_headers is set.
 * @param r     The request whose response is being started.
 * @param h3ctx The per-request H3 context receiving the snapshot.
 */
void h3_response_finalize(request_rec* r, h3_conn_ctx_t* h3ctx);

#endif /* !H3_HAS_RESPONSE_BUCKETS */

#endif /* H3_RESPONSE_COMPAT_H */
