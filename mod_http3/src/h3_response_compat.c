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
 *
 * Response finalization for httpd 2.4.x, where responses have no
 * ap_bucket_response representation. Adapted from Apache httpd's
 * ap_http_header_filter (modules/http/http_filters.c) by way of
 * mod_http2's h2_c2_filter.c create_response(), minus the HTTP/1.x
 * transport concerns (keepalive, chunked transfer encoding).
 */

#include "h3_response_compat.h"

#if !H3_HAS_RESPONSE_BUCKETS

    #include <http_core.h>
    #include <http_protocol.h>
    #include <util_time.h>

    #include <apr_lib.h>
    #include <apr_strings.h>
    #include <apr_tables.h>

    #include <string.h>

    #include "h3_check.h"

static int uniq_field_values(void* d, const char* /*key*/, const char* val)
{
    apr_array_header_t* values = d;
    char* start;
    char* e = apr_pstrdup(values->pool, val);

    do
    {
        while (*e == ',' || apr_isspace(*e))
        {
            ++e;
        }
        if (*e == '\0')
        {
            break;
        }
        start = e;
        while (*e != '\0' && *e != ',' && !apr_isspace(*e))
        {
            ++e;
        }
        if (*e != '\0')
        {
            *e++ = '\0';
        }

        int i;
        char** strpp;
        for (i = 0, strpp = (char**)values->elts; i < values->nelts; ++i, ++strpp)
        {
            if (*strpp && ap_cstr_casecmp(*strpp, start) == 0)
            {
                break;
            }
        }
        if (i == values->nelts)
        {
            *(char**)apr_array_push(values) = start;
        }
    } while (*e != '\0');

    return 1;
}

/* Some clients choke on multiple Vary fields or duplicate tokens; combine
 * multiples and drop duplicates. */
static void fix_vary(request_rec* r)
{
    apr_array_header_t* varies = apr_array_make(r->pool, 5, sizeof(char*));
    apr_table_do(uniq_field_values, varies, r->headers_out, "Vary", NULL);
    if (varies->nelts > 0)
    {
        apr_table_setn(r->headers_out, "Vary", apr_array_pstrcat(r->pool, varies, ','));
    }
}

void h3_response_finalize(request_rec* r, h3_conn_ctx_t* h3ctx)
{
    CHECK(r);
    CHECK(h3ctx);
    if (h3ctx->resp_headers)
    {
        return;
    }

    /* Combine the two header field tables so later set/unset operations are
     * not bypassed. */
    if (!apr_is_empty_table(r->err_headers_out))
    {
        r->headers_out = apr_table_overlay(r->pool, r->err_headers_out, r->headers_out);
        apr_table_clear(r->err_headers_out);
    }

    if (apr_table_get(r->subprocess_env, "force-no-vary") != NULL)
    {
        apr_table_unset(r->headers_out, "Vary");
    }
    else
    {
        fix_vary(r);
    }

    /* Remove any ETag response header field if earlier processing says so
     * (such as a 'FileETag None' directive). */
    if (apr_table_get(r->notes, "no-etag") != NULL)
    {
        apr_table_unset(r->headers_out, "ETag");
    }

    if (AP_STATUS_IS_HEADER_ONLY(r->status))
    {
        apr_table_unset(r->headers_out, "Transfer-Encoding");
        apr_table_unset(r->headers_out, "Content-Length");
        r->content_type = r->content_encoding = NULL;
        r->content_languages = NULL;
        r->clength = r->chunked = 0;
    }

    const char* ctype = ap_make_content_type(r, r->content_type);
    if (ctype)
    {
        apr_table_setn(r->headers_out, "Content-Type", ctype);
    }

    if (r->content_encoding)
    {
        apr_table_setn(r->headers_out, "Content-Encoding", r->content_encoding);
    }

    if (!apr_is_empty_array(r->content_languages))
    {
        const char* field = apr_table_get(r->headers_out, "Content-Language");
        char* token;
        while (field && (token = ap_get_list_item(r->pool, &field)) != NULL)
        {
            int i;
            char** languages = (char**)r->content_languages->elts;
            for (i = 0; i < r->content_languages->nelts; ++i)
            {
                if (!ap_cstr_casecmp(token, languages[i]))
                {
                    break;
                }
            }
            if (i == r->content_languages->nelts)
            {
                *((char**)apr_array_push(r->content_languages)) = token;
            }
        }
        apr_table_setn(r->headers_out, "Content-Language", apr_array_pstrcat(r->pool, r->content_languages, ','));
    }

    /* Control cachability for non-cachable responses if not already set by
     * some other part of the server configuration. */
    if (r->no_cache && !apr_table_get(r->headers_out, "Expires"))
    {
        char* date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
        ap_recent_rfc822_date(date, r->request_time);
        apr_table_add(r->headers_out, "Expires", date);
    }

    /* Handlers that shortcut HEAD requests leave the content-length filter
     * computing a spurious zero Content-Length; suppress it. */
    const char* clheader = apr_table_get(r->headers_out, "Content-Length");
    if (r->header_only && clheader && !strcmp(clheader, "0"))
    {
        apr_table_unset(r->headers_out, "Content-Length");
    }

    /* Keep the set-by-proxy Date and Server headers, otherwise generate
     * fresh ones. */
    if (r->proxyreq == PROXYREQ_NONE || !apr_table_get(r->headers_out, "Date"))
    {
        char* date = apr_palloc(r->pool, APR_RFC822_DATE_LEN);
        ap_recent_rfc822_date(date, r->request_time);
        apr_table_setn(r->headers_out, "Date", date);
    }
    if (r->proxyreq == PROXYREQ_NONE || !apr_table_get(r->headers_out, "Server"))
    {
        const char* us = ap_get_server_banner();
        if (us && *us)
        {
            apr_table_setn(r->headers_out, "Server", us);
        }
    }

    h3ctx->resp_status = r->status;
    h3ctx->resp_headers = r->headers_out;
}

#endif /* !H3_HAS_RESPONSE_BUCKETS */
