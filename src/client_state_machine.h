/**
 * @file client_state_machine.h
 * @brief Client State Machine - Orchestrates client behavior
 * 
 * This module implements the main state machine for the gaming client.
 * It coordinates between button handler, VPN controller, WebSocket client,
 * and LED controller to provide the complete client functionality.
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#ifndef CLIENT_STATE_MACHINE_H
#define CLIENT_STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ClientStateMachine Client State Machine
 * @brief Main state machine for client operations
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** VPN connection timeout in seconds */
#define CLIENT_VPN_CONNECT_TIMEOUT_S    30

/** WebSocket connection timeout in seconds */
#define CLIENT_WS_CONNECT_TIMEOUT_S     10

/** PS5 query timeout in seconds */
#define CLIENT_PS5_QUERY_TIMEOUT_S      5

/** LED update duration in seconds */
#define CLIENT_LED_UPDATE_DURATION_S    2

/** Maximum retry attempts for operations */
#define CLIENT_MAX_RETRY_ATTEMPTS       3

/** Retry interval in seconds */
#define CLIENT_RETRY_INTERVAL_S         5

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief Client state machine states
 */
typedef enum {
    CLIENT_STATE_IDLE = 0,          /**< Idle, waiting for button press */
    CLIENT_STATE_VPN_CONNECTING,    /**< Connecting to VPN */
    CLIENT_STATE_VPN_CONNECTED,     /**< VPN connected */
    CLIENT_STATE_WS_CONNECTING,     /**< Connecting to WebSocket server */
    CLIENT_STATE_QUERYING_PS5,      /**< Querying PS5 status */
    CLIENT_STATE_LED_UPDATE,        /**< Updating LED based on PS5 status */
    CLIENT_STATE_WAITING,           /**< Waiting before returning to idle */
    CLIENT_STATE_ERROR,             /**< Error state */
    CLIENT_STATE_CLEANUP,           /**< Cleaning up resources */
} client_state_t;

/**
 * @brief PS5 status from server
 */
typedef enum {
    PS5_STATUS_UNKNOWN = 0,         /**< Status unknown */
    PS5_STATUS_OFF,                 /**< PS5 is off */
    PS5_STATUS_STANDBY,             /**< PS5 is in standby */
    PS5_STATUS_ON,                  /**< PS5 is on */
} ps5_status_t;

/**
 * @brief Client error codes
 */
typedef enum {
    CLIENT_ERROR_NONE = 0,          /**< No error */
    CLIENT_ERROR_VPN_TIMEOUT,       /**< VPN connection timeout */
    CLIENT_ERROR_VPN_FAILED,        /**< VPN connection failed */
    CLIENT_ERROR_WS_TIMEOUT,        /**< WebSocket connection timeout */
    CLIENT_ERROR_WS_FAILED,         /**< WebSocket connection failed */
    CLIENT_ERROR_PS5_TIMEOUT,       /**< PS5 query timeout */
    CLIENT_ERROR_PS5_FAILED,        /**< PS5 query failed */
    CLIENT_ERROR_MAX_RETRIES,       /**< Maximum retries exceeded */
} client_error_t;

/**
 * @brief Client statistics
 */
typedef struct {
    uint32_t button_press_count;    /**< Total button presses */
    uint32_t successful_queries;    /**< Successful PS5 queries */
    uint32_t failed_queries;        /**< Failed PS5 queries */
    uint32_t vpn_connect_count;     /**< VPN connection attempts */
    uint32_t vpn_success_count;     /**< Successful VPN connections */
    uint32_t error_count;           /**< Total errors */
    time_t last_query_time;         /**< Last successful query timestamp */
} client_stats_t;

/**
 * @brief Client context structure
 * 
 * This structure holds all state and configuration for the client
 * state machine. It should be treated as opaque outside this module.
 */
typedef struct client_context_t client_context_t;

/**
 * @brief Client state change callback
 * 
 * @param old_state Previous state
 * @param new_state New state
 * @param user_data User-provided data pointer
 */
typedef void (*client_state_callback_t)(client_state_t old_state,
                                        client_state_t new_state,
                                        void *user_data);

/**
 * @brief Client error callback
 * 
 * @param error Error code
 * @param message Error message
 * @param user_data User-provided data pointer
 */
typedef void (*client_error_callback_t)(client_error_t error,
                                        const char *message,
                                        void *user_data);

/**
 * @brief Client configuration
 */
typedef struct {
    int button_pin;                 /**< GPIO pin for button */
    int button_debounce_ms;         /**< Button debounce time */
    char vpn_socket_path[256];      /**< VPN agent socket path */
    char ws_server_host[256];       /**< WebSocket server host */
    int ws_server_port;             /**< WebSocket server port */
    bool auto_retry;                /**< Enable automatic retry on error */
    int max_retry_attempts;         /**< Maximum retry attempts */
} client_config_t;

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Create and initialize client context
 * 
 * Allocate and initialize a new client context structure.
 * 
 * @param config Configuration structure (NULL for defaults)
 * @return Pointer to client context, or NULL on failure
 */
client_context_t* client_sm_create(const client_config_t *config);

/**
 * @brief Initialize client state machine
 * 
 * Initialize the state machine and all sub-modules.
 * 
 * @param ctx Client context
 * @return 0 on success, negative error code on failure
 */
int client_sm_init(client_context_t *ctx);

/**
 * @brief Set state change callback
 * 
 * Register callback for state transitions.
 * 
 * @param ctx Client context
 * @param callback Callback function
 * @param user_data User data for callback
 */
void client_sm_set_state_callback(client_context_t *ctx,
                                  client_state_callback_t callback,
                                  void *user_data);

/**
 * @brief Set error callback
 * 
 * Register callback for errors.
 * 
 * @param ctx Client context
 * @param callback Callback function
 * @param user_data User data for callback
 */
void client_sm_set_error_callback(client_context_t *ctx,
                                  client_error_callback_t callback,
                                  void *user_data);

/**
 * @brief Update state machine (non-blocking)
 * 
 * Process one iteration of the state machine.
 * Should be called regularly from main event loop.
 * 
 * @param ctx Client context
 * @return 0 on success, negative error code on failure
 */
int client_sm_update(client_context_t *ctx);

/**
 * @brief Run state machine (blocking)
 * 
 * Enter blocking loop that continuously updates the state machine.
 * 
 * @param ctx Client context
 * @return 0 on normal exit, negative error code on failure
 */
int client_sm_run(client_context_t *ctx);

/**
 * @brief Stop state machine
 * 
 * Signal the state machine to stop and return to idle.
 * 
 * @param ctx Client context
 */
void client_sm_stop(client_context_t *ctx);

/**
 * @brief Trigger button press event
 * 
 * Manually trigger a button press (for testing or external control).
 * 
 * @param ctx Client context
 * @param long_press true for long press, false for short press
 * @return 0 on success, negative error code on failure
 */
int client_sm_trigger_button(client_context_t *ctx, bool long_press);

/**
 * @brief Get current state
 * 
 * @param ctx Client context
 * @return Current state
 */
client_state_t client_sm_get_state(const client_context_t *ctx);

/**
 * @brief Get last PS5 status
 * 
 * @param ctx Client context
 * @return Last received PS5 status
 */
ps5_status_t client_sm_get_ps5_status(const client_context_t *ctx);

/**
 * @brief Set PS5 status (for testing)
 * 
 * Manually set PS5 status to test LED behavior.
 * 
 * @param ctx Client context
 * @param status PS5 status to set
 */
void client_sm_set_ps5_status(client_context_t *ctx, ps5_status_t status);

/**
 * @brief Get last error
 * 
 * @param ctx Client context
 * @return Last error code
 */
client_error_t client_sm_get_last_error(const client_context_t *ctx);

/**
 * @brief Get statistics
 * 
 * Retrieve client statistics.
 * 
 * @param ctx Client context
 * @param stats Pointer to stats structure to fill
 * @return 0 on success, negative error code on failure
 */
int client_sm_get_stats(const client_context_t *ctx, client_stats_t *stats);

/**
 * @brief Reset statistics
 * 
 * Reset all statistics counters to zero.
 * 
 * @param ctx Client context
 */
void client_sm_reset_stats(client_context_t *ctx);

/**
 * @brief Clean up client state machine
 * 
 * Clean up all resources and sub-modules.
 * 
 * @param ctx Client context
 */
void client_sm_cleanup(client_context_t *ctx);

/**
 * @brief Destroy client context
 * 
 * Free the client context structure.
 * 
 * @param ctx Client context to destroy
 */
void client_sm_destroy(client_context_t *ctx);

/**
 * @brief Get state string
 * 
 * @param state Client state
 * @return State description string
 */
const char* client_state_to_string(client_state_t state);

/**
 * @brief Get PS5 status string
 * 
 * @param status PS5 status
 * @return Status description string
 */
const char* ps5_status_to_string(ps5_status_t status);

/**
 * @brief Get error string
 * 
 * @param error Error code
 * @return Error description string
 */
const char* client_error_to_string(client_error_t error);

/** @} */ // end of ClientStateMachine group

#ifdef __cplusplus
}
#endif

#endif /* CLIENT_STATE_MACHINE_H */
