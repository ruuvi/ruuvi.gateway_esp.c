/**
 * User config file to reduce SSL in/out content len for testing.
 *
 * Usage:
 *   cmake .. -DMBEDTLS_USER_CONFIG_FILE="../tests/configs/user-config-reduced-buffer-size.h"
 */

#define MBEDTLS_SSL_IN_CONTENT_LEN 16384
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096

/* Marker macro to enable conditional test selection for reduced buffer size.
 * Tests can use depends_on:MBEDTLS_SSL_REDUCED_OUT_BUFFER to run only with
 * this config, or depends_on:!MBEDTLS_SSL_REDUCED_OUT_BUFFER for default config. */
#define MBEDTLS_SSL_REDUCED_OUT_BUFFER
