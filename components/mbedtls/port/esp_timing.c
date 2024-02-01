/*
 * Portable interface to the CPU cycle counter
 *
 * SPDX-FileCopyrightText: The Mbed TLS Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2016-2022 Espressif Systems (Shanghai) CO LTD
 */
/*
 * mbedtls_timing_get_timer()m mbedtls_timing_set_delay() and
 * mbedtls_timing_set_delay only abstracted from mbedtls/library/timing.c
 * as that does not build on ESP-IDF but these 2 functions are needed for
 * DTLS (in particular mbedtls_ssl_set_timer_cb() must be called for DTLS
 * which requires these 2 delay functions).
 */

#include <mbedtls/build_info.h>

#if !defined(MBEDTLS_ESP_TIMING_C)

#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "mbedtls/timing.h"

struct _hr_time
{
    TickType_t tick_start;
};

unsigned long mbedtls_timing_get_timer( struct mbedtls_timing_hr_time *val, int reset )
{
    struct _hr_time *t = (struct _hr_time *) val;

    if( reset )
    {
        t->tick_start = xTaskGetTickCount();
        return( 0 );
    }
    else
    {
        const TickType_t tick_now = xTaskGetTickCount();
        const unsigned long delta = pdTICKS_TO_MS( tick_now - t->tick_start );
        return( delta );
    }
}

/*
 * Set delays to watch
 */
void mbedtls_timing_set_delay( void *data, uint32_t int_ms, uint32_t fin_ms )
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;

    ctx->MBEDTLS_PRIVATE(int_ms) = int_ms;
    ctx->MBEDTLS_PRIVATE(fin_ms) = fin_ms;

    if( fin_ms != 0 )
        (void) mbedtls_timing_get_timer( &ctx->MBEDTLS_PRIVATE(timer), 1 );
}

/*
 * Get number of delays expired
 */
int mbedtls_timing_get_delay( void *data )
{
    mbedtls_timing_delay_context *ctx = (mbedtls_timing_delay_context *) data;
    unsigned long elapsed_ms;

    if( ctx->MBEDTLS_PRIVATE(fin_ms) == 0 )
        return( -1 );

    elapsed_ms = mbedtls_timing_get_timer( &ctx->MBEDTLS_PRIVATE(timer), 0 );

    if( elapsed_ms >= ctx->MBEDTLS_PRIVATE(fin_ms) )
        return( 2 );

    if( elapsed_ms >= ctx->MBEDTLS_PRIVATE(int_ms) )
        return( 1 );

    return( 0 );
}

/*
 * Get the final delay.
 */
uint32_t mbedtls_timing_get_final_delay( const mbedtls_timing_delay_context *data )
{
    return( data->MBEDTLS_PRIVATE(fin_ms) );
}

#endif /* MBEDTLS_ESP_TIMING_C */
