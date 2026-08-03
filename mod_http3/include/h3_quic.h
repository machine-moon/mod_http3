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

#ifndef H3_QUIC_H
#define H3_QUIC_H

#include <apr_pools.h>

/**
 * Make @p name the engine every later call dispatches to.
 * @param name Engine name, matched case-insensitively.
 * @return 1 if this build contains @p name, 0 otherwise. On 0 the previous
 *         selection is left untouched.
 */
int quic_select(const char* name);

/**
 * Name of the engine currently selected.
 * @return Engine name; the build's default until quic_select() succeeds.
 */
const char* quic_engine_name(void);

/**
 * List the engines this build contains, for diagnostics.
 * @param pool Pool the returned string is allocated from.
 * @return Comma-separated names, for example "openssl, ngtcp2".
 */
const char* quic_engine_names(apr_pool_t* pool);

#endif /* H3_QUIC_H */
