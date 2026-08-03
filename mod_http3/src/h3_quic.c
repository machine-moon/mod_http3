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

#include <apr_cstr.h>
#include <apr_strings.h>

#include "h3_quic.h"
#include "quic_ossl.h"

#ifdef H3_ENABLE_NGTCP2
    #include "quic_ngtcp2.h"
#endif

typedef struct quic_engine_entry
{
    const char* name;
    const quic_ops* (*ops)(void);
} quic_engine_entry;

static const quic_engine_entry quic_engines[] = {
    {"openssl", quic_ossl_ops},
#ifdef H3_ENABLE_NGTCP2
    {"ngtcp2", quic_ngtcp2_ops},
#endif
};

#define QUIC_ENGINE_COUNT (sizeof(quic_engines) / sizeof(quic_engines[0]))

static size_t quic_active;

int quic_select(const char* name)
{
    for (size_t i = 0; i < QUIC_ENGINE_COUNT; i++)
    {
        if (apr_cstr_casecmp(name, quic_engines[i].name) == 0)
        {
            quic_active = i;
            return 1;
        }
    }
    return 0;
}

const char* quic_engine_name(void)
{
    return quic_engines[quic_active].name;
}

const char* quic_engine_names(apr_pool_t* pool)
{
    const char* list = quic_engines[0].name;
    for (size_t i = 1; i < QUIC_ENGINE_COUNT; i++)
    {
        list = apr_pstrcat(pool, list, ", ", quic_engines[i].name, NULL);
    }
    return list;
}

const quic_ops* quic_get_ops(void)
{
    return quic_engines[quic_active].ops();
}
