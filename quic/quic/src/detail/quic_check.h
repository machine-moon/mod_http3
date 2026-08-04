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

#ifndef QUIC_CHECK_H
#define QUIC_CHECK_H

#include <stdio.h>
#include <stdlib.h>

/**
 * Abort unless @p expr_ holds, reporting it on stderr. For invariants a caller
 * cannot recover from; recoverable failures belong in the engine's error buffer.
 * @param expr_ Condition that must hold.
 */
#define QUIC_CHECK(expr_)                                                                       \
    do                                                                                          \
    {                                                                                           \
        if (!(expr_))                                                                           \
        {                                                                                       \
            fprintf(stderr, "quic: check failed: %s at %s:%d\n", #expr_, __FILE__, __LINE__);   \
            abort();                                                                            \
        }                                                                                       \
    } while (0)

#endif /* QUIC_CHECK_H */
