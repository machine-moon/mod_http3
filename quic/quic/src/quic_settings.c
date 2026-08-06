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

#include "quic.h"

void quic_settings_default(quic_settings* s)
{
    if (!s)
    {
        return;
    }
    *s = (quic_settings){
        .initial_max_data = 1024 * 1024,
        .initial_max_stream_data_bidi_local = 256 * 1024,
        .initial_max_stream_data_bidi_remote = 256 * 1024,
        .initial_max_stream_data_uni = 256 * 1024,
        .initial_max_streams_bidi = 128,
        .initial_max_streams_uni = 8,
        .max_idle_timeout_ms = 30 * 1000,
        .cc_algo = QUIC_CC_DEFAULT,
        .enable_datagrams = 0,
        .address_validation = 1,
        .session_tickets = 1,
        .early_data = 0,
    };
}
