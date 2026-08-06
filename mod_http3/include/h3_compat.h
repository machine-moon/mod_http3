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

#ifndef H3_COMPAT_H
#define H3_COMPAT_H

#include <ap_mmn.h>

/**
 * httpd trunk (MMN 20211221.6+, same boundary mod_http2 uses) hands response
 * meta data to protocol modules as ap_bucket_response buckets and keeps the
 * HTTP/1.x serialization filters off slave connections. It also carries the
 * extra conn_rec fields (slaves, requests, async_filter).
 *
 * On httpd 2.4.x none of that exists: the core HTTP_HEADER filter serializes
 * the response as HTTP/1.x text instead. There mod_http3 removes that filter
 * from its synthesized requests and snapshots status and headers itself via
 * h3_response_finalize() (see h3_response_compat.c), mirroring mod_http2's
 * !AP_HAS_RESPONSE_BUCKETS code path.
 */
#if AP_MODULE_MAGIC_AT_LEAST(20211221, 6)
    #define H3_HAS_RESPONSE_BUCKETS 1
#else
    #define H3_HAS_RESPONSE_BUCKETS 0
#endif

#endif /* H3_COMPAT_H */
