/* Licensed under the Apache License, Version 2.0. */

#include <httpd.h>
#include <http_config.h>
#include <http_protocol.h>

#include <apr_atomic.h>
#include <apr_time.h>

#include <string.h>

static volatile apr_uint32_t flood_active = 0;

static int aptest_handler(request_rec* r)
{
    if (!r->handler)
    {
        return DECLINED;
    }

    if (strcmp(r->handler, "aptest-active-floods") == 0)
    {
        ap_set_content_type(r, "text/plain");
        ap_rprintf(r, "%u\n", (unsigned)apr_atomic_read32(&flood_active));
        return OK;
    }

    if (strcmp(r->handler, "aptest-flood-stream") == 0)
    {
        char chunk[16 * 1024];
        memset(chunk, 'x', sizeof(chunk));
        apr_atomic_inc32(&flood_active);
        ap_set_content_type(r, "application/octet-stream");
        ap_rputs("first-chunk\n", r);
        ap_rflush(r);
        for (int i = 0; i < 4096; i++)
        {
            if (ap_rwrite(chunk, sizeof(chunk), r) < 0 || ap_rflush(r) != APR_SUCCESS)
            {
                break;
            }
        }
        apr_atomic_dec32(&flood_active);
        return OK;
    }

    if (strcmp(r->handler, "aptest-slow-stream") != 0)
    {
        return DECLINED;
    }

    ap_set_content_type(r, "text/plain");
    ap_rputs("first-chunk\n", r);
    ap_rflush(r);
    apr_sleep(apr_time_from_sec(3));
    ap_rputs("second-chunk\n", r);
    return OK;
}

static void aptest_hooks(apr_pool_t* pool)
{
    (void)pool;
    ap_hook_handler(aptest_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

AP_DECLARE_MODULE(aptest) = {STANDARD20_MODULE_STUFF,    NULL, NULL, NULL, NULL, NULL, aptest_hooks,
#if defined(AP_MODULE_FLAG_NONE)
                             AP_MODULE_FLAG_ALWAYS_MERGE
#endif
};
