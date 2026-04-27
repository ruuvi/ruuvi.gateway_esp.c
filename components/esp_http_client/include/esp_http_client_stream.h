/**
 * @file esp_http_client_stream.h
 * @author TheSomeMan
 * @date 2026-04-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP_HTTP_CLIENT_STREAM_H
#define ESP_HTTP_CLIENT_STREAM_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {

#endif

/**
 * @brief Commands for the HTTP stream reader interface.
 */
typedef enum http_stream_reader_cmd {
    HTTP_STREAM_READER_CMD_OPEN, /*!< Initialize or open the stream. */
    HTTP_STREAM_READER_CMD_READ, /*!< Read data from the stream. */
    HTTP_STREAM_READER_CMD_CLOSE, /*!< Close or clean up the stream. */
} http_stream_reader_cmd_e;

typedef struct http_stream_last_call http_stream_last_call_t;

/**
 * @brief Argument union for the HTTP stream reader interface.
 *
 * This union provides command-specific arguments for the stream reader callback.
 * - For OPEN:
 *       - p_param is the initialization parameter (e.g., pointer to a string),
 *       - p_last_call is a pointer to a structure that tracks the last stream reader called.
 * - For READ: p_buf is the buffer to read into, buf_len is its length.
 * - For CLOSE: no arguments.
 */
typedef union http_stream_reader_arg {
    struct {
        void *p_param; /*!< Parameter for OPEN command (e.g., pointer to source string). */
        http_stream_last_call_t *p_last_call; /*!< Pointer to last call tracking structure. */
    } open;

    struct {
        char *const p_buf; /*!< Buffer to read data into (for READ command). */
        const size_t buf_len; /*!< Length of the buffer. */
    } read;

    struct {
        /* No arguments for CLOSE command. */
    } close;
} http_stream_reader_arg_t;

/**
 * @brief Context structure for string-based HTTP stream reader.
 *
 * Used by http_stream_reader_string to track the source string, current offset, and last call tracking.
 */
typedef struct http_stream_reader_string_ctx {
    const char *p_str; /*!< The source string to read from */
    size_t data_offset; /*!< The current offset in the source string */;
    http_stream_last_call_t *p_last_call; /*!< Pointer to last call tracking structure. */
} http_stream_reader_string_ctx_t;

/**
 * @brief Function pointer type for HTTP client stream reader callback.
 *
 * This callback is used to perform stream operations (open, read, close) for HTTP path/query/header data streams.
 *
 * @param cmd     The command to execute (OPEN, READ, or CLOSE), of type http_stream_reader_cmd_e.
 * @param arg     Command-specific arguments, provided as a http_stream_reader_arg_t union.
 * @param p_ctx   User-defined context pointer, e.g., for maintaining state between calls.
 *
 * @return
 *   - For READ: Number of bytes read into p_buf, or negative value on error.
 *   - For OPEN/CLOSE: 0 on success or negative on error.
 */
typedef ssize_t (*http_stream_reader_t)(const http_stream_reader_cmd_e cmd,
                                        const http_stream_reader_arg_t arg,
                                        void *const p_ctx);

/**
 * @brief Structure to track the last stream reader called.
 *
 * Used to store the callback and context of the last stream reader, allowing for chaining or cleanup.
 */
struct http_stream_last_call {
    http_stream_reader_t cb_stream_reader; /*!< Last stream reader callback used. */
    void *p_ctx; /*!< Context pointer for the last stream reader. */
};

/**
 * @brief String-based implementation of the http_stream_reader_t interface.
 *
 * This function allows reading from a C string as a stream, supporting OPEN, READ, and CLOSE commands.
 * The context must be a pointer to a http_stream_reader_string_ctx_t structure.
 *
 * - OPEN: Initializes the context with the source string (arg.open.p_param) and optionally sets up last call tracking.
 * - READ: Reads data from the current offset in the string into arg.read.p_buf, up to arg.read.buf_len.
 *         Updates the offset in the context. Ensures null-termination.
 * - CLOSE: Resets the context and clears last call tracking if set.
 *
 * @param cmd   The stream reader command (OPEN, READ, or CLOSE).
 * @param arg   Command-specific arguments (see http_stream_reader_arg_t).
 * @param p_ctx Pointer to a http_stream_reader_string_ctx_t context.
 *
 * @return
 *   - On READ: Number of bytes written to p_buf (excluding null terminator), or -1 on error.
 *   - On OPEN/CLOSE: 0 on success or negative on error.
 */
ssize_t http_stream_reader_string(const http_stream_reader_cmd_e cmd,
                                  const http_stream_reader_arg_t arg,
                                  void *const p_ctx);

/**
 * @brief Helper to initialize a string stream reader context.
 *
 * Calls http_stream_reader_string with the OPEN command to set up the context for reading.
 * Optionally sets up last call tracking.
 *
 * @param p_ctx Pointer to the string stream reader context to initialize.
 * @param p_str Pointer to the source string to read from.
 * @param p_stream_reader_last_call Pointer to last call tracking structure (can be NULL).
 */
void http_stream_reader_string_open(http_stream_reader_string_ctx_t *const p_ctx, const char *const p_str,
                                    http_stream_last_call_t *const p_stream_reader_last_call);

/**
 * @brief Helper to close a string stream reader context.
 *
 * Calls http_stream_reader_string with the CLOSE command to reset the context and clear last call tracking.
 *
 * @param p_ctx Pointer to the string stream reader context to close.
 */
void http_stream_reader_string_close(http_stream_reader_string_ctx_t *const p_ctx);

/**
 * @brief Helper to open (initialize) a generic stream reader context.
 *
 * Calls the specified stream_reader with the OPEN command to initialize the context for reading.
 * Optionally sets up last call tracking.
 *
 * @param stream_reader             The stream reader function to call.
 * @param p_ctx                     Context pointer to initialize.
 * @param p_param                   Initialization parameter (e.g., pointer to a string or resource).
 * @param p_stream_reader_last_call Pointer to last call tracking structure (can be NULL).
 * @return
 *   - true on success (stream opened/initialized).
 *   - false on error (e.g., stream_reader returns negative value).
 */
bool http_stream_reader_wrap_open(http_stream_reader_t stream_reader, void *const p_ctx, void *const p_param,
                                  http_stream_last_call_t *const p_stream_reader_last_call);

/**
 * @brief Helper to close a generic stream reader context.
 *
 * Calls the specified stream_reader with the CLOSE command to clean up or reset the context.
 *
 * @param stream_reader The stream reader function to call.
 * @param p_ctx         Context pointer to close/reset.
 */
void http_stream_reader_wrap_close(http_stream_reader_t stream_reader, void *const p_ctx);

/**
 * @brief Wraps a stream reader to fill a buffer, handling offsets and buffer fullness.
 *
 * This function calls the provided stream_reader to read data into p_buf, starting at p_buf_offset,
 * and updates the buffer offset and fullness state accordingly. Ensures the buffer is null-terminated.
 *
 * @param stream_reader   The stream reader function to call.
 * @param p_ctx           Context pointer to pass to the stream reader.
 * @param p_buf           Buffer to fill with data.
 * @param buf_len         Total length of p_buf.
 * @param p_buf_offset    Pointer to the current offset in p_buf to write to.
 * @param p_is_buf_full   Pointer to a bool that will be set to true if the buffer is full.
 *
 * @return
 *   - true on success (buffer filled or operation complete).
 *   - false on error (e.g., stream_reader returns negative value).
 */
bool http_stream_reader_wrap_read(http_stream_reader_t stream_reader,
                                  void *const p_ctx,
                                  char *const p_buf,
                                  const ssize_t buf_len,
                                  size_t *const p_buf_offset,
                                  bool *const p_is_buf_full);

/**
 * @brief Convenience wrapper for reading from a string stream reader.
 *
 * Calls http_stream_reader_wrap_read using http_stream_reader_string as the stream reader.
 *
 * @param p_ctx         Pointer to the string stream reader context.
 * @param p_buf         Buffer to fill with data.
 * @param buf_len       Total length of p_buf.
 * @param p_buf_offset  Pointer to the current offset in p_buf to write to.
 * @param p_is_buf_full Pointer to a bool that will be set to true if the buffer is full.
 *
 * @return
 *   - true on success (buffer filled or operation complete).
 *   - false on error.
 */
bool http_stream_reader_wrap_read_string(void *const p_ctx,
                                         char *const p_buf,
                                         const ssize_t buf_len,
                                         size_t *const p_buf_offset,
                                         bool *const p_is_buf_full);

#ifdef __cplusplus
}
#endif

#endif // ESP_HTTP_CLIENT_STREAM_H
