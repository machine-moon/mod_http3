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

#include <strings.h>

#include "detail/quic_check.h"
#include "quic.h"
#include "quic_null.h"
#include "quic_ossl.h"

#ifdef H3_ENABLE_NGTCP2
    #include "quic_ngtcp2.h"
#endif

typedef struct
{
    const char* name;
    const quic_api* (*api)(void);
} quic_entry;

/* Index 0 is the default. Adding an engine is one row, and nothing outside quic/. */
static const quic_entry engines[] = {
    {"openssl", quic_ossl_api},
#ifdef H3_ENABLE_NGTCP2
    {"ngtcp2", quic_ngtcp2_api},
#endif
    {"null", quic_null_api},
};

#define ENGINE_COUNT (sizeof(engines) / sizeof(engines[0]))

static size_t active;

size_t quic_engine_count(void)
{
    return ENGINE_COUNT;
}

const char* quic_engine_name_at(size_t i)
{
    return i < ENGINE_COUNT ? engines[i].name : NULL;
}

int quic_select(const char* name)
{
    QUIC_CHECK(name);
    for (size_t i = 0; i < ENGINE_COUNT; i++)
    {
        if (strcasecmp(name, engines[i].name) == 0)
        {
            active = i;
            return 1;
        }
    }
    return 0;
}

const char* quic_engine_name(void)
{
    return engines[active].name;
}

const quic_api* quic_selected(void)
{
    return engines[active].api();
}
