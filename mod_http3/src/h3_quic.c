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

#include "quic_ossl.h"

typedef struct quic_engine_entry
{
    const char* name;
    const quic_ops* (*ops)(void);
} quic_engine_entry;

static const quic_engine_entry quic_engines[] = {
    {"openssl", quic_ossl_ops},
};

const quic_ops* quic_get_ops(void)
{
    return quic_engines[0].ops();
}
