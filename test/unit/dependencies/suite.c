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

#include "sput.h"

void run_dependencies_suite(void)
{
    sput_enter_suite("dependencies");

    extern void run_nghttp3_tests(void);
    run_nghttp3_tests();

    extern void run_apr_tests(void);
    run_apr_tests();

    extern void run_apu_tests(void);
    run_apu_tests();

    extern void run_httpd_tests(void);
    run_httpd_tests();

    extern void run_openssl_tests(void);
    run_openssl_tests();

#ifdef H3_ENABLE_NGTCP2
    extern void run_ngtcp2_tests(void);
    run_ngtcp2_tests();
#endif
}
