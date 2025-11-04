/**
 * @file vpn_controller.h
 * @brief VPN Controller Module - Interface with VPN Agent via Unix Socket
 * 
 * This module provides communication with the VPN agent to control
 * VPN connections. Features include:
 * - Connect/Disconnect commands
 * - State query
 * - Timeout and retry mechanism
 * - Non-blocking I/O
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#ifndef VPN_CONTROLLER_H
#define VPN_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup VPNController VPN Controller Module
 * @brief VPN connection control and state management
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** Default VPN agent socket path */
#define VPN_AGENT_SOCKET_PATH       "/var/run/vpn-agent.sock"

/** Connection timeout in milliseconds */
#define VPN_CONNECT_TIMEOUT_MS      30000

/** Command timeout in milliseconds */
#define VPN_COMMAND_TIMEOUT_MS      5000

/** Maximum retry attempts */
#define VPN_MAX_RETRY_ATTEMPTS      3

/** Retry interval in milliseconds */
#define VPN_RETRY_INTERVAL_MS       5000

/** Maximum message size */
#define VPN_MAX_MESSAGE_SIZE        1024

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief VPN connection states
 */
typedef enum {
    VPN_STATE_UNKNOWN = 0,          /**< State unknown or not initialized */
    VPN_STATE_DISCONNECTED,         /**< VPN is disconnected */
    VPN_STATE_CONNECTING,           /**< VPN is connecting */
    VPN_STATE_CONNECTED,            /**< VPN is connected */
    VPN_STATE_DISCONNECTING,        /**< VPN is disconnecting */
    VPN_STATE_ERROR,                /**< VPN is in error state */
} vpn_state_t;

/**
 * @brief VPN error codes
 */
typedef enum {
    VPN_ERROR_NONE = 0,             /**< No error */
    VPN_ERROR_SOCKET,               /**< Socket operation failed */
    VPN_ERROR_TIMEOUT,              /**< Operation timed out */
    VPN_ERROR_AGENT_UNREACHABLE,    /**< VPN agent is not reachable */
    VPN_ERROR_INVALID_RESPONSE,     /**< Invalid response from agent */
    VPN_ERROR_ALREADY_CONNECTED,    /**< Already connected */
    VPN_ERROR_NOT_CONNECTED,        /**< Not connected */
    VPN_ERROR_MAX_RETRIES,          /**< Maximum retries exceeded */
} vpn_error_t;

/**
 * @brief VPN connection information
 */
typedef struct {
    vpn_state_t state;              /**< Current VPN state */
    char server_ip[64];             /**< Connected server IP */
    char local_ip[64];              /**< Local VPN IP */
    uint32_t bytes_sent;            /**< Bytes sent through VPN */
    uint32_t bytes_received;        /**< Bytes received through VPN */
    uint32_t connect_time;          /**< Connection timestamp */
} vpn_info_t;

/**
 * @brief VPN state change callback
 * 
 * @param old_state Previous VPN state
 * @param new_state New VPN state
 * @param user_data User-provided data pointer
 */
typedef void (*vpn_state_callback_t)(vpn_state_t old_state, 
                                     vpn_state_t new_state, 
                                     void *user_data);

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize the VPN controller
 * 
 * Initialize the VPN controller and establish connection to the VPN agent.
 * 
 * @param socket_path Path to VPN agent Unix socket (NULL for default)
 * @return 0 on success, negative error code on failure
 * 
 * @note Default socket path is /var/run/vpn-agent.sock
 */
int vpn_controller_init(const char *socket_path);

/**
 * @brief Set VPN state change callback
 * 
 * Register a callback function to be notified of VPN state changes.
 * 
 * @param callback Callback function pointer
 * @param user_data User data to pass to callback (can be NULL)
 */
void vpn_controller_set_callback(vpn_state_callback_t callback, void *user_data);

/**
 * @brief Connect to VPN
 * 
 * Send connect command to VPN agent. This function is non-blocking.
 * Use vpn_controller_get_state() to check connection status.
 * 
 * @return 0 on success, negative error code on failure
 * 
 * @note This function returns immediately, actual connection is asynchronous
 * @note Use callback or polling to monitor connection state
 */
int vpn_controller_connect(void);

/**
 * @brief Disconnect from VPN
 * 
 * Send disconnect command to VPN agent. This function is non-blocking.
 * 
 * @return 0 on success, negative error code on failure
 */
int vpn_controller_disconnect(void);

/**
 * @brief Get current VPN state
 * 
 * Query the current VPN connection state from the agent.
 * 
 * @return Current VPN state
 */
vpn_state_t vpn_controller_get_state(void);

/**
 * @brief Get detailed VPN information
 * 
 * Retrieve detailed information about the VPN connection.
 * 
 * @param info Pointer to vpn_info_t structure to fill
 * @return 0 on success, negative error code on failure
 */
int vpn_controller_get_info(vpn_info_t *info);

/**
 * @brief Check if VPN is connected
 * 
 * @return true if VPN is in CONNECTED state, false otherwise
 */
bool vpn_controller_is_connected(void);

/**
 * @brief Wait for VPN state
 * 
 * Block until VPN reaches the specified state or timeout occurs.
 * 
 * @param target_state Target state to wait for
 * @param timeout_ms Timeout in milliseconds
 * @return 0 if target state reached, negative error code on timeout or failure
 */
int vpn_controller_wait_for_state(vpn_state_t target_state, int timeout_ms);

/**
 * @brief Process VPN controller events (non-blocking)
 * 
 * Process incoming messages from VPN agent and update state.
 * Should be called regularly from main event loop.
 * 
 * @param timeout_ms Timeout for waiting on socket (0 for non-blocking)
 * @return 0 on success, negative error code on failure
 */
int vpn_controller_process(int timeout_ms);

/**
 * @brief Get last error code
 * 
 * @return Last error code
 */
vpn_error_t vpn_controller_get_last_error(void);

/**
 * @brief Get error string
 * 
 * Convert error code to human-readable string.
 * 
 * @param error Error code
 * @return Error description string
 */
const char* vpn_controller_error_to_string(vpn_error_t error);

/**
 * @brief Get state string
 * 
 * Convert VPN state to human-readable string.
 * 
 * @param state VPN state
 * @return State description string
 */
const char* vpn_controller_state_to_string(vpn_state_t state);

/**
 * @brief Clean up VPN controller resources
 * 
 * Disconnect from VPN agent and release all resources.
 */
void vpn_controller_cleanup(void);

/** @} */ // end of VPNController group

#ifdef __cplusplus
}
#endif

#endif /* VPN_CONTROLLER_H */
