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

#ifdef H3_ENABLE_NGTCP2

    #include "sput.h"
    #include <ngtcp2/ngtcp2.h>
    #include <ngtcp2/ngtcp2_crypto.h>
    #include <ngtcp2/ngtcp2_crypto_ossl.h>

static void test_ngtcp2_version(void)
{
    const ngtcp2_info* info = ngtcp2_version(NGTCP2_VERSION_AGE);
    sput_fail_unless(info != NULL, "ngtcp2_version returns info");
    sput_fail_unless(info->version_num == 0x011900, "ngtcp2 version == 1.25.0");
    sput_fail_unless(strcmp(info->version_str, "1.25.0") == 0, "ngtcp2 version string == 1.25.0");
}

static void test_ngtcp2_settings_default(void)
{
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    sput_fail_unless(settings.max_tx_udp_payload_size > 0, "max_tx_udp_payload_size > 0");

    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    sput_fail_unless(params.active_connection_id_limit > 0, "active_connection_id_limit > 0");
}

/* Both the crypto helper and the OpenSSL API it needs are found by symbol probe. */
static void test_ngtcp2_crypto_ossl_available(void)
{
    sput_fail_unless(ngtcp2_crypto_ossl_init() == 0, "ngtcp2_crypto_ossl_init succeeds");

    ngtcp2_crypto_ossl_ctx* ctx = NULL;
    sput_fail_unless(ngtcp2_crypto_ossl_ctx_new(&ctx, NULL) == 0, "ossl ctx created");
    sput_fail_unless(ctx != NULL, "ossl ctx not NULL");
    ngtcp2_crypto_ossl_ctx_del(ctx);
}

void run_ngtcp2_tests(void)
{
    sput_run_test(test_ngtcp2_version);
    sput_run_test(test_ngtcp2_settings_default);
    sput_run_test(test_ngtcp2_crypto_ossl_available);
}

#endif /* H3_ENABLE_NGTCP2 */
