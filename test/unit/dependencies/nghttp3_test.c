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
#include <nghttp3/nghttp3.h>

static void test_nghttp3_version(void)
{
    const nghttp3_info* info = nghttp3_version(NGHTTP3_VERSION_AGE);
    sput_fail_unless(info != NULL, "nghttp3_version returns info");
    sput_fail_unless(info->version_num == 0x011200, "nghttp3 version == 1.18.0");
    sput_fail_unless(strcmp(info->version_str, "1.18.0") == 0, "nghttp3 version string == 1.18.0");
}

static void test_nghttp3_settings_default(void)
{
    nghttp3_settings settings;
    nghttp3_settings_default(&settings);
    sput_fail_unless(settings.max_field_section_size > 0, "max_field_section_size > 0");
}

static void test_nghttp3_default_allocator(void)
{
    const nghttp3_mem* mem = nghttp3_mem_default();
    sput_fail_unless(mem != NULL, "default allocator not NULL");
    sput_fail_unless(mem->malloc != NULL, "malloc not NULL");
    sput_fail_unless(mem->free != NULL, "free not NULL");
    sput_fail_unless(mem->realloc != NULL, "realloc not NULL");
}

void run_nghttp3_tests(void)
{
    sput_run_test(test_nghttp3_version);
    sput_run_test(test_nghttp3_settings_default);
    sput_run_test(test_nghttp3_default_allocator);
}
