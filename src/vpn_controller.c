/**
 * @file vpn_controller.c
 * @brief VPN Controller Implementation
 * 
 * This module provides VPN connection control via Unix socket communication
 * with the VPN agent. Features include:
 * - Non-blocking socket communication
 * - Automatic retry with exponential backoff
 * - State management with callbacks
 * - Timeout detection
 * - JSON command/response handling
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#include "vpn_controller.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/socket_helper.h>
    #include <gaming/logger.h>
  #else
    #include "../../gaming-core/src/socket_helper.h"
    #include "../../gaming-core/src/logger.h"
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

/* ============================================================
 *  Internal Structures
 * ============================================================ */

/**
 * @brief VPN controller context
 */
typedef struct {
    bool initialized;
    int sockfd;
    char socket_path[256];
    vpn_state_t current_state;
    vpn_state_t previous_state;
    
    // Callback
    vpn_state_callback_t callback;
    void *user_data;
    
    // Connection info
    vpn_info_t info;
    
    // Retry mechanism
    int retry_count;
    uint32_t last_retry_time;
    uint32_t retry_interval;
    
    // Timeout
    uint32_t operation_start_time;
    uint32_t operation_timeout;
    
    // Pending operation
    bool operation_pending;
    char pending_command[VPN_MAX_MESSAGE_SIZE];
} vpn_controller_ctx_t;

/* ============================================================
 *  Global Variables
 * ============================================================ */

static vpn_controller_ctx_t g_vpn_ctx = {
    .initialized = false,
    .sockfd = -1,
    .current_state = VPN_STATE_UNKNOWN,
    .previous_state = VPN_STATE_UNKNOWN,
    .callback = NULL,
    .user_data = NULL,
    .retry_count = 0,
    .retry_interval = VPN_RETRY_INTERVAL_MS,
    .operation_pending = false,
};

/* ============================================================
 *  Internal Helper Functions
 * ============================================================ */

/**
 * @brief Get current time in milliseconds
 */
static uint32_t get_current_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

/**
 * @brief Check if timeout occurred
 */
static bool is_timeout(uint32_t start_time, uint32_t timeout_ms) {
    uint32_t current_time = get_current_time_ms();
    return (current_time - start_time) >= timeout_ms;
}

/**
 * @brief Change VPN state and trigger callback
 */
static void change_state(vpn_state_t new_state) {
    if (g_vpn_ctx.current_state == new_state) {
        return;
    }
    
    g_vpn_ctx.previous_state = g_vpn_ctx.current_state;
    g_vpn_ctx.current_state = new_state;
    
    #ifndef TESTING
    logger_info("VPN state changed: %s -> %s",
             vpn_controller_state_to_string(g_vpn_ctx.previous_state),
             vpn_controller_state_to_string(new_state));
    #endif
    
    // Trigger callback
    if (g_vpn_ctx.callback != NULL) {
        g_vpn_ctx.callback(g_vpn_ctx.previous_state, new_state, g_vpn_ctx.user_data);
    }
}

/**
 * @brief Connect to VPN agent socket
 */
static int connect_to_agent(void) {
    if (g_vpn_ctx.sockfd >= 0) {
        #ifndef TESTING
        socket_helper_close(g_vpn_ctx.sockfd);
        #else
        close(g_vpn_ctx.sockfd);
        #endif
        g_vpn_ctx.sockfd = -1;
    }
    
    #ifndef TESTING
    g_vpn_ctx.sockfd = socket_helper_connect_unix(g_vpn_ctx.socket_path);
    #else
    // In test mode, simulate socket creation
    g_vpn_ctx.sockfd = 100;  // Mock socket fd
    #endif
    
    if (g_vpn_ctx.sockfd < 0) {
        #ifndef TESTING
        logger_error("Failed to connect to VPN agent: %s", g_vpn_ctx.socket_path);
        #endif
        return -1;
    }
    
    // Set non-blocking mode
    #ifndef TESTING
    socket_helper_set_nonblocking(g_vpn_ctx.sockfd);
    socket_helper_set_timeout(g_vpn_ctx.sockfd, VPN_COMMAND_TIMEOUT_MS / 1000);
    #endif
    
    return 0;
}

/**
 * @brief Send JSON command to VPN agent
 */
static int send_command(const char *action) {
    if (g_vpn_ctx.sockfd < 0) {
        if (connect_to_agent() < 0) {
            return -1;
        }
    }
    
    // Build JSON command
    char command[VPN_MAX_MESSAGE_SIZE];
    snprintf(command, sizeof(command), "{\"action\":\"%s\"}\n", action);
    
    #ifndef TESTING
    ssize_t sent = socket_helper_send(g_vpn_ctx.sockfd, command, strlen(command));
    #else
    ssize_t sent = strlen(command);  // Mock send
    #endif
    
    if (sent < 0) {
        #ifndef TESTING
        logger_error("Failed to send VPN command: %s", action);
        #endif
        return -1;
    }
    
    #ifndef TESTING
    logger_debug("VPN command sent: %s", action);
    #endif
    
    return 0;
}

/**
 * @brief Receive and parse response from VPN agent
 */
static int receive_response(char *response, size_t max_len) {
    if (g_vpn_ctx.sockfd < 0) {
        return -1;
    }
    
    #ifndef TESTING
    ssize_t received = socket_helper_recv(g_vpn_ctx.sockfd, response, max_len - 1);
    #else
    // Mock response in test mode
    const char *mock_response = "{\"status\":\"ok\",\"state\":\"connected\"}\n";
    strncpy(response, mock_response, max_len - 1);
    ssize_t received = strlen(response);
    #endif
    
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;  // No data yet
        }
        #ifndef TESTING
        logger_error("Failed to receive VPN response");
        #endif
        return -1;
    }
    
    if (received == 0) {
        // Connection closed
        #ifndef TESTING
        logger_warning("VPN agent closed connection");
        #endif
        return -1;
    }
    
    response[received] = '\0';
    
    #ifndef TESTING
    logger_debug("VPN response received: %s", response);
    #endif
    
    return (int)received;
}

/**
 * @brief Parse VPN state from JSON response
 */
static vpn_state_t parse_state_from_response(const char *response) {
    // Simple JSON parsing (in production, use a proper JSON library)
    if (strstr(response, "\"state\":\"connected\"")) {
        return VPN_STATE_CONNECTED;
    } else if (strstr(response, "\"state\":\"connecting\"")) {
        return VPN_STATE_CONNECTING;
    } else if (strstr(response, "\"state\":\"disconnecting\"")) {
        return VPN_STATE_DISCONNECTING;
    } else if (strstr(response, "\"state\":\"disconnected\"")) {
        return VPN_STATE_DISCONNECTED;
    } else if (strstr(response, "\"state\":\"error\"")) {
        return VPN_STATE_ERROR;
    }
    
    return VPN_STATE_UNKNOWN;
}

/**
 * @brief Parse connection info from JSON response
 */
static void parse_info_from_response(const char *response, vpn_info_t *info) {
    // Simple parsing - in production use proper JSON library
    const char *ptr;
    
    // Parse server IP
    ptr = strstr(response, "\"server_ip\":\"");
    if (ptr) {
        ptr += 13;  // Skip "server_ip":"
        sscanf(ptr, "%63[^\"]", info->server_ip);
    }
    
    // Parse local IP
    ptr = strstr(response, "\"local_ip\":\"");
    if (ptr) {
        ptr += 12;  // Skip "local_ip":"
        sscanf(ptr, "%63[^\"]", info->local_ip);
    }
    
    // Parse bytes sent
    ptr = strstr(response, "\"bytes_sent\":");
    if (ptr) {
        sscanf(ptr + 13, "%u", &info->bytes_sent);
    }
    
    // Parse bytes received
    ptr = strstr(response, "\"bytes_received\":");
    if (ptr) {
        sscanf(ptr + 17, "%u", &info->bytes_received);
    }
    
    info->state = parse_state_from_response(response);
}

/**
 * @brief Handle retry logic
 */
static bool should_retry(void) {
    if (g_vpn_ctx.retry_count >= VPN_MAX_RETRY_ATTEMPTS) {
        return false;
    }
    
    uint32_t current_time = get_current_time_ms();
    if (current_time - g_vpn_ctx.last_retry_time < g_vpn_ctx.retry_interval) {
        return false;
    }
    
    return true;
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

int vpn_controller_init(const char *socket_path) {
    if (g_vpn_ctx.initialized) {
        return -1;  // Already initialized
    }
    
    // Use default path if not provided
    if (socket_path == NULL) {
        strncpy(g_vpn_ctx.socket_path, VPN_AGENT_SOCKET_PATH, 
                sizeof(g_vpn_ctx.socket_path) - 1);
    } else {
        strncpy(g_vpn_ctx.socket_path, socket_path, 
                sizeof(g_vpn_ctx.socket_path) - 1);
    }
    
    g_vpn_ctx.sockfd = -1;
    g_vpn_ctx.current_state = VPN_STATE_DISCONNECTED;
    g_vpn_ctx.previous_state = VPN_STATE_UNKNOWN;
    g_vpn_ctx.retry_count = 0;
    g_vpn_ctx.operation_pending = false;
    g_vpn_ctx.initialized = true;
    
    memset(&g_vpn_ctx.info, 0, sizeof(vpn_info_t));
    g_vpn_ctx.info.state = VPN_STATE_DISCONNECTED;
    
    #ifndef TESTING
    logger_info("VPN controller initialized: %s", g_vpn_ctx.socket_path);
    #endif
    
    return 0;
}

void vpn_controller_set_callback(vpn_state_callback_t callback, void *user_data) {
    g_vpn_ctx.callback = callback;
    g_vpn_ctx.user_data = user_data;
}

int vpn_controller_connect(void) {
    if (!g_vpn_ctx.initialized) {
        return -1;
    }
    
    if (g_vpn_ctx.current_state == VPN_STATE_CONNECTED) {
        return -1;  // Already connected
    }
    
    if (g_vpn_ctx.current_state == VPN_STATE_CONNECTING) {
        return -1;  // Connection in progress
    }
    
    // Send connect command
    if (send_command("connect") < 0) {
        change_state(VPN_STATE_ERROR);
        return -1;
    }
    
    // Update state
    change_state(VPN_STATE_CONNECTING);
    
    g_vpn_ctx.operation_start_time = get_current_time_ms();
    g_vpn_ctx.operation_timeout = VPN_CONNECT_TIMEOUT_MS;
    g_vpn_ctx.operation_pending = true;
    g_vpn_ctx.retry_count = 0;
    
    strncpy(g_vpn_ctx.pending_command, "connect", sizeof(g_vpn_ctx.pending_command) - 1);
    
    return 0;
}

int vpn_controller_disconnect(void) {
    if (!g_vpn_ctx.initialized) {
        return -1;
    }
    
    if (g_vpn_ctx.current_state == VPN_STATE_DISCONNECTED) {
        return 0;  // Already disconnected
    }
    
    // Send disconnect command
    if (send_command("disconnect") < 0) {
        change_state(VPN_STATE_ERROR);
        return -1;
    }
    
    change_state(VPN_STATE_DISCONNECTING);
    
    g_vpn_ctx.operation_start_time = get_current_time_ms();
    g_vpn_ctx.operation_timeout = VPN_COMMAND_TIMEOUT_MS;
    g_vpn_ctx.operation_pending = true;
    
    strncpy(g_vpn_ctx.pending_command, "disconnect", sizeof(g_vpn_ctx.pending_command) - 1);
    
    return 0;
}

vpn_state_t vpn_controller_get_state(void) {
    return g_vpn_ctx.current_state;
}

int vpn_controller_get_info(vpn_info_t *info) {
    if (!g_vpn_ctx.initialized || info == NULL) {
        return -1;
    }
    
    // Send status query
    if (send_command("status") < 0) {
        return -1;
    }
    
    // Receive response
    char response[VPN_MAX_MESSAGE_SIZE];
    int received = receive_response(response, sizeof(response));
    
    if (received <= 0) {
        return -1;
    }
    
    // Parse response
    parse_info_from_response(response, info);
    
    // Update cached info
    memcpy(&g_vpn_ctx.info, info, sizeof(vpn_info_t));
    
    return 0;
}

const char* vpn_controller_state_to_string(vpn_state_t state) {
    switch (state) {
        case VPN_STATE_UNKNOWN:        return "UNKNOWN";
        case VPN_STATE_DISCONNECTED:   return "DISCONNECTED";
        case VPN_STATE_CONNECTING:     return "CONNECTING";
        case VPN_STATE_CONNECTED:      return "CONNECTED";
        case VPN_STATE_DISCONNECTING:  return "DISCONNECTING";
        case VPN_STATE_ERROR:          return "ERROR";
        default:                       return "INVALID";
    }
}

const char* vpn_controller_error_to_string(vpn_error_t error) {
    switch (error) {
        case VPN_ERROR_NONE:                return "NO_ERROR";
        case VPN_ERROR_SOCKET:              return "SOCKET_ERROR";
        case VPN_ERROR_TIMEOUT:             return "TIMEOUT";
        case VPN_ERROR_AGENT_UNREACHABLE:   return "AGENT_UNREACHABLE";
        case VPN_ERROR_INVALID_RESPONSE:    return "INVALID_RESPONSE";
        case VPN_ERROR_ALREADY_CONNECTED:   return "ALREADY_CONNECTED";
        case VPN_ERROR_NOT_CONNECTED:       return "NOT_CONNECTED";
        case VPN_ERROR_MAX_RETRIES:         return "MAX_RETRIES_EXCEEDED";
        default:                            return "UNKNOWN_ERROR";
    }
}

int vpn_controller_process(int timeout_ms) {
    if (!g_vpn_ctx.initialized) {
        return -1;
    }
    
    // Note: timeout_ms parameter is for future use with select/poll
    // Currently using simple timeout checking
    
    // Check if there's a pending operation
    if (!g_vpn_ctx.operation_pending) {
        return 0;
    }
    
    // Check for timeout
    if (is_timeout(g_vpn_ctx.operation_start_time, g_vpn_ctx.operation_timeout)) {
        #ifndef TESTING
        logger_warning("VPN operation timeout");
        #endif
        
        // Retry if possible
        if (should_retry()) {
            g_vpn_ctx.retry_count++;
            g_vpn_ctx.last_retry_time = get_current_time_ms();
            g_vpn_ctx.operation_start_time = get_current_time_ms();
            
            #ifndef TESTING
            logger_info("VPN retry attempt %d/%d", g_vpn_ctx.retry_count, VPN_MAX_RETRY_ATTEMPTS);
            #endif
            
            // Resend command
            if (send_command(g_vpn_ctx.pending_command) < 0) {
                change_state(VPN_STATE_ERROR);
                g_vpn_ctx.operation_pending = false;
                return -1;
            }
            
            return 0;
        } else {
            // Max retries exceeded
            change_state(VPN_STATE_ERROR);
            g_vpn_ctx.operation_pending = false;
            return -1;
        }
    }
    
    // Try to receive response
    char response[VPN_MAX_MESSAGE_SIZE];
    int received = receive_response(response, sizeof(response));
    
    if (received < 0) {
        // Error occurred
        change_state(VPN_STATE_ERROR);
        g_vpn_ctx.operation_pending = false;
        return -1;
    }
    
    if (received == 0) {
        // No data yet, continue waiting
        return 0;
    }
    
    // Parse state from response
    vpn_state_t new_state = parse_state_from_response(response);
    
    if (new_state != VPN_STATE_UNKNOWN) {
        change_state(new_state);
        g_vpn_ctx.operation_pending = false;
        g_vpn_ctx.retry_count = 0;
    }
    
    return 0;
}

void vpn_controller_cleanup(void) {
    if (!g_vpn_ctx.initialized) {
        return;
    }
    
    // Close socket
    if (g_vpn_ctx.sockfd >= 0) {
        #ifndef TESTING
        socket_helper_close(g_vpn_ctx.sockfd);
        #else
        close(g_vpn_ctx.sockfd);
        #endif
        g_vpn_ctx.sockfd = -1;
    }
    
    // Reset state
    g_vpn_ctx.initialized = false;
    g_vpn_ctx.current_state = VPN_STATE_UNKNOWN;
    g_vpn_ctx.callback = NULL;
    g_vpn_ctx.user_data = NULL;
    g_vpn_ctx.operation_pending = false;
    
    #ifndef TESTING
    logger_info("VPN controller cleaned up");
    #endif
}
