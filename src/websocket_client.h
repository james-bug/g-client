/**
 * @file websocket_client.h
 * @brief WebSocket Client Module - Connection to gaming-server WebSocket
 * 
 * This module provides WebSocket client functionality to communicate with
 * the gaming-server. Features include:
 * - Connect to WebSocket server
 * - Send/receive JSON messages
 * - Auto-reconnect with exponential backoff
 * - Ping/Pong heartbeat
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#ifndef WEBSOCKET_CLIENT_H
#define WEBSOCKET_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup WebSocketClient WebSocket Client Module
 * @brief WebSocket client for server communication
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** Default WebSocket server port */
#define WS_DEFAULT_SERVER_PORT      8765

/** Maximum message size in bytes */
#define WS_MAX_MESSAGE_SIZE         4096

/** Connection timeout in milliseconds */
#define WS_CONNECT_TIMEOUT_MS       10000

/** Ping interval in milliseconds */
#define WS_PING_INTERVAL_MS         30000

/** Pong timeout in milliseconds */
#define WS_PONG_TIMEOUT_MS          5000

/** Initial reconnect delay in milliseconds */
#define WS_RECONNECT_DELAY_INIT_MS  1000

/** Maximum reconnect delay in milliseconds */
#define WS_RECONNECT_DELAY_MAX_MS   60000

/** Reconnect backoff multiplier */
#define WS_RECONNECT_BACKOFF        2

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief WebSocket connection states
 */
typedef enum {
    WS_STATE_DISCONNECTED = 0,      /**< Not connected */
    WS_STATE_CONNECTING,            /**< Connection in progress */
    WS_STATE_CONNECTED,             /**< Connected and ready */
    WS_STATE_DISCONNECTING,         /**< Disconnection in progress */
    WS_STATE_ERROR,                 /**< Error state */
} ws_state_t;

/**
 * @brief WebSocket error codes
 */
typedef enum {
    WS_ERROR_NONE = 0,              /**< No error */
    WS_ERROR_INIT,                  /**< Initialization failed */
    WS_ERROR_CONNECT,               /**< Connection failed */
    WS_ERROR_SEND,                  /**< Send failed */
    WS_ERROR_RECEIVE,               /**< Receive failed */
    WS_ERROR_TIMEOUT,               /**< Operation timed out */
    WS_ERROR_PROTOCOL,              /**< Protocol error */
    WS_ERROR_CLOSED,                /**< Connection closed */
} ws_error_t;

/**
 * @brief WebSocket message callback
 * 
 * Called when a message is received from the server.
 * 
 * @param message Message content (null-terminated string)
 * @param length Message length in bytes
 * @param user_data User-provided data pointer
 */
typedef void (*ws_message_callback_t)(const char *message, 
                                      size_t length, 
                                      void *user_data);

/**
 * @brief WebSocket connected callback
 * 
 * Called when connection is established.
 * 
 * @param user_data User-provided data pointer
 */
typedef void (*ws_connected_callback_t)(void *user_data);

/**
 * @brief WebSocket disconnected callback
 * 
 * Called when connection is lost or closed.
 * 
 * @param reason Disconnect reason
 * @param user_data User-provided data pointer
 */
typedef void (*ws_disconnected_callback_t)(const char *reason, void *user_data);

/**
 * @brief WebSocket error callback
 * 
 * Called when an error occurs.
 * 
 * @param error Error code
 * @param message Error message
 * @param user_data User-provided data pointer
 */
typedef void (*ws_error_callback_t)(ws_error_t error, 
                                    const char *message, 
                                    void *user_data);

/**
 * @brief WebSocket client configuration
 */
typedef struct {
    char server_host[256];          /**< Server hostname or IP */
    int server_port;                /**< Server port */
    bool auto_reconnect;            /**< Enable auto-reconnect */
    int ping_interval_ms;           /**< Ping interval in milliseconds */
    int connect_timeout_ms;         /**< Connection timeout in milliseconds */
} ws_config_t;

/**
 * @brief WebSocket statistics
 */
typedef struct {
    uint32_t messages_sent;         /**< Total messages sent */
    uint32_t messages_received;     /**< Total messages received */
    uint32_t bytes_sent;            /**< Total bytes sent */
    uint32_t bytes_received;        /**< Total bytes received */
    uint32_t reconnect_count;       /**< Number of reconnections */
    uint32_t error_count;           /**< Number of errors */
    uint32_t last_ping_ms;          /**< Last ping round-trip time */
} ws_stats_t;

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize WebSocket client
 * 
 * Initialize the WebSocket client with server address.
 * 
 * @param server_host Server hostname or IP address
 * @param server_port Server port number
 * @return 0 on success, negative error code on failure
 */
int ws_client_init(const char *server_host, int server_port);

/**
 * @brief Initialize with configuration
 * 
 * Initialize the WebSocket client with detailed configuration.
 * 
 * @param config Configuration structure
 * @return 0 on success, negative error code on failure
 */
int ws_client_init_with_config(const ws_config_t *config);

/**
 * @brief Set callback functions
 * 
 * Register callback functions for various events.
 * 
 * @param on_connected Connected callback (can be NULL)
 * @param on_disconnected Disconnected callback (can be NULL)
 * @param on_message Message callback (can be NULL)
 * @param on_error Error callback (can be NULL)
 * @param user_data User data for callbacks (can be NULL)
 */
void ws_client_set_callbacks(
    ws_connected_callback_t on_connected,
    ws_disconnected_callback_t on_disconnected,
    ws_message_callback_t on_message,
    ws_error_callback_t on_error,
    void *user_data
);

/**
 * @brief Connect to WebSocket server
 * 
 * Initiate connection to the WebSocket server.
 * This function is non-blocking, use callbacks to detect connection status.
 * 
 * @return 0 on success, negative error code on failure
 */
int ws_client_connect(void);

/**
 * @brief Send message to server
 * 
 * Send a text message to the WebSocket server.
 * 
 * @param message Message to send (null-terminated string)
 * @return 0 on success, negative error code on failure
 * 
 * @note Message must be valid JSON for this application
 */
int ws_client_send(const char *message);

/**
 * @brief Send binary data to server
 * 
 * Send binary data to the WebSocket server.
 * 
 * @param data Data buffer
 * @param length Data length in bytes
 * @return 0 on success, negative error code on failure
 */
int ws_client_send_binary(const uint8_t *data, size_t length);

/**
 * @brief Process WebSocket events (non-blocking)
 * 
 * Process incoming/outgoing WebSocket traffic and trigger callbacks.
 * This function should be called regularly from the main event loop.
 * 
 * @param timeout_ms Timeout for waiting on events (0 for non-blocking)
 * @return 0 on success, negative error code on failure
 */
int ws_client_service(int timeout_ms);

/**
 * @brief Get connection state
 * 
 * @return Current WebSocket connection state
 */
ws_state_t ws_client_get_state(void);

/**
 * @brief Check if connected
 * 
 * @return true if connected, false otherwise
 */
bool ws_client_is_connected(void);

/**
 * @brief Enable or disable auto-reconnect
 * 
 * @param enable true to enable, false to disable
 */
void ws_client_set_auto_reconnect(bool enable);

/**
 * @brief Disconnect from server
 * 
 * Close the WebSocket connection gracefully.
 * 
 * @return 0 on success, negative error code on failure
 */
int ws_client_disconnect(void);

/**
 * @brief Get statistics
 * 
 * Retrieve WebSocket client statistics.
 * 
 * @param stats Pointer to stats structure to fill
 * @return 0 on success, negative error code on failure
 */
int ws_client_get_stats(ws_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * Reset all statistics counters to zero.
 */
void ws_client_reset_stats(void);

/**
 * @brief Get last error
 * 
 * @return Last error code
 */
ws_error_t ws_client_get_last_error(void);

/**
 * @brief Get error string
 * 
 * Convert error code to human-readable string.
 * 
 * @param error Error code
 * @return Error description string
 */
const char* ws_client_error_to_string(ws_error_t error);

/**
 * @brief Get state string
 * 
 * Convert state to human-readable string.
 * 
 * @param state WebSocket state
 * @return State description string
 */
const char* ws_client_state_to_string(ws_state_t state);

/**
 * @brief Clean up WebSocket client
 * 
 * Disconnect and release all resources.
 */
void ws_client_cleanup(void);

/* ============================================================
 *  Helper Functions for Gaming System Messages
 * ============================================================ */

/**
 * @brief Send PS5 status query
 * 
 * Send a query message to request PS5 status from server.
 * 
 * @return 0 on success, negative error code on failure
 */
int ws_client_query_ps5_status(void);

/**
 * @brief Send heartbeat message
 * 
 * Send a heartbeat message to keep connection alive.
 * 
 * @return 0 on success, negative error code on failure
 */
int ws_client_send_heartbeat(void);

/** @} */ // end of WebSocketClient group

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_CLIENT_H */
