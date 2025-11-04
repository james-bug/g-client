/**
 * @file websocket_client.c
 * @brief WebSocket Client Implementation
 * 
 * This module provides WebSocket client functionality for communicating
 * with the gaming server. Features include:
 * - Non-blocking WebSocket connection
 * - Automatic reconnection with exponential backoff
 * - Ping/Pong heartbeat mechanism
 * - JSON message handling
 * - Multiple callback support
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#include "websocket_client.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/logger.h>
  #else
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

#ifndef TESTING
#include <libwebsockets.h>
#endif

/* ============================================================
 *  Internal Structures
 * ============================================================ */

/**
 * @brief WebSocket client context
 */
typedef struct {
    bool initialized;
    char server_host[256];
    int server_port;
    
    ws_state_t current_state;
    ws_state_t previous_state;
    
    // Callbacks
    ws_connected_callback_t on_connected;
    ws_disconnected_callback_t on_disconnected;
    ws_error_callback_t on_error;
    ws_message_callback_t on_message;
    void *user_data;
    
    // Reconnection
    bool auto_reconnect;
    int reconnect_attempts;
    uint32_t reconnect_interval;
    uint32_t last_reconnect_time;
    uint32_t max_reconnect_interval;
    
    // Heartbeat
    uint32_t last_ping_time;
    uint32_t ping_interval;
    bool waiting_for_pong;
    
    // libwebsockets context (only in real mode)
    #ifndef TESTING
    struct lws_context *ws_context;
    struct lws *ws_connection;
    #else
    void *ws_context;
    void *ws_connection;
    #endif
    
    // Message buffer
    char send_buffer[WS_MAX_MESSAGE_SIZE];
    size_t send_buffer_len;
    
    char recv_buffer[WS_MAX_MESSAGE_SIZE];
    size_t recv_buffer_len;
    
} ws_client_ctx_t;

/* ============================================================
 *  Global Variables
 * ============================================================ */

static ws_client_ctx_t g_ws_ctx = {
    .initialized = false,
    .current_state = WS_STATE_DISCONNECTED,
    .previous_state = WS_STATE_DISCONNECTED,
    .auto_reconnect = true,
    .reconnect_attempts = 0,
    .reconnect_interval = WS_RECONNECT_INTERVAL_MS,
    .max_reconnect_interval = WS_MAX_RECONNECT_INTERVAL_MS,
    .ping_interval = WS_PING_INTERVAL_MS,
    .waiting_for_pong = false,
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
 * @brief Change WebSocket state and trigger callback
 */
static void change_state(ws_state_t new_state) {
    if (g_ws_ctx.current_state == new_state) {
        return;
    }
    
    g_ws_ctx.previous_state = g_ws_ctx.current_state;
    g_ws_ctx.current_state = new_state;
    
    #ifndef TESTING
    logger_info("WebSocket state changed: %s -> %s",
             ws_client_state_to_string(g_ws_ctx.previous_state),
             ws_client_state_to_string(new_state));
    #endif
    
    // Trigger appropriate callbacks
    if (new_state == WS_STATE_CONNECTED && g_ws_ctx.on_connected != NULL) {
        g_ws_ctx.on_connected(g_ws_ctx.user_data);
    } else if (new_state == WS_STATE_DISCONNECTED && g_ws_ctx.on_disconnected != NULL) {
        // 修正: 添加 reason 參數
        g_ws_ctx.on_disconnected("Disconnected", g_ws_ctx.user_data);
    } else if (new_state == WS_STATE_ERROR && g_ws_ctx.on_error != NULL) {
        // 修正: 使用正確的錯誤碼 WS_ERROR_CONNECT
        g_ws_ctx.on_error(WS_ERROR_CONNECT, "Connection error", g_ws_ctx.user_data);
    }
}

/**
 * @brief Calculate reconnection interval with exponential backoff
 */
static uint32_t calculate_reconnect_interval(void) {
    uint32_t interval = g_ws_ctx.reconnect_interval * (1 << g_ws_ctx.reconnect_attempts);
    
    if (interval > g_ws_ctx.max_reconnect_interval) {
        interval = g_ws_ctx.max_reconnect_interval;
    }
    
    return interval;
}

/**
 * @brief Check if should attempt reconnection
 */
static bool should_reconnect(void) {
    if (!g_ws_ctx.auto_reconnect) {
        return false;
    }
    
    if (g_ws_ctx.reconnect_attempts >= WS_MAX_RECONNECT_ATTEMPTS) {
        return false;
    }
    
    uint32_t current_time = get_current_time_ms();
    uint32_t interval = calculate_reconnect_interval();
    
    if (current_time - g_ws_ctx.last_reconnect_time < interval) {
        return false;
    }
    
    return true;
}

#ifndef TESTING
/**
 * @brief libwebsockets callback
 */
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            change_state(WS_STATE_CONNECTED);
            g_ws_ctx.reconnect_attempts = 0;
            g_ws_ctx.last_ping_time = get_current_time_ms();
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (len > 0 && len < WS_MAX_MESSAGE_SIZE) {
                memcpy(g_ws_ctx.recv_buffer, in, len);
                g_ws_ctx.recv_buffer[len] = '\0';
                g_ws_ctx.recv_buffer_len = len;
                
                if (g_ws_ctx.on_message != NULL) {
                    // 修正: 添加 length 參數
                    g_ws_ctx.on_message(g_ws_ctx.recv_buffer, len, g_ws_ctx.user_data);
                }
            }
            break;
            
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (g_ws_ctx.send_buffer_len > 0) {
                unsigned char buf[LWS_PRE + WS_MAX_MESSAGE_SIZE];
                memcpy(&buf[LWS_PRE], g_ws_ctx.send_buffer, g_ws_ctx.send_buffer_len);
                
                lws_write(wsi, &buf[LWS_PRE], g_ws_ctx.send_buffer_len, LWS_WRITE_TEXT);
                g_ws_ctx.send_buffer_len = 0;
            }
            break;
            
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            logger_error("WebSocket connection error");
            change_state(WS_STATE_ERROR);
            break;
            
        case LWS_CALLBACK_CLOSED:
            change_state(WS_STATE_DISCONNECTED);
            g_ws_ctx.ws_connection = NULL;
            break;
            
        case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
            g_ws_ctx.waiting_for_pong = false;
            break;
            
        default:
            break;
    }
    
    return 0;
}
#endif

/**
 * @brief Attempt to connect to WebSocket server
 */
static int attempt_connect(void) {
    #ifndef TESTING
    struct lws_client_connect_info connect_info;
    memset(&connect_info, 0, sizeof(connect_info));
    
    connect_info.context = g_ws_ctx.ws_context;
    connect_info.address = g_ws_ctx.server_host;
    connect_info.port = g_ws_ctx.server_port;
    connect_info.path = "/";
    connect_info.host = g_ws_ctx.server_host;
    connect_info.origin = g_ws_ctx.server_host;
    connect_info.protocol = NULL;
    
    g_ws_ctx.ws_connection = lws_client_connect_via_info(&connect_info);
    
    if (g_ws_ctx.ws_connection == NULL) {
        logger_error("Failed to connect to WebSocket server");
        return -1;
    }
    #else
    // Mock connection in test mode
    g_ws_ctx.ws_connection = (void*)0x1234;  // Non-null pointer
    change_state(WS_STATE_CONNECTED);
    #endif
    
    return 0;
}

/**
 * @brief Send ping to server
 */
static void send_ping(void) {
    #ifndef TESTING
    if (g_ws_ctx.ws_connection != NULL) {
        unsigned char buf[LWS_PRE + 125];
        lws_write(g_ws_ctx.ws_connection, &buf[LWS_PRE], 0, LWS_WRITE_PING);
        g_ws_ctx.waiting_for_pong = true;
    }
    #endif
    
    g_ws_ctx.last_ping_time = get_current_time_ms();
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

int ws_client_init(const char *server_host, int server_port) {
    if (g_ws_ctx.initialized) {
        return -1;  // Already initialized
    }
    
    if (server_host == NULL || server_port <= 0 || server_port > 65535) {
        return -1;
    }
    
    // Copy server info
    strncpy(g_ws_ctx.server_host, server_host, sizeof(g_ws_ctx.server_host) - 1);
    g_ws_ctx.server_port = server_port;
    
    // Initialize state
    g_ws_ctx.current_state = WS_STATE_DISCONNECTED;
    g_ws_ctx.previous_state = WS_STATE_DISCONNECTED;
    g_ws_ctx.reconnect_attempts = 0;
    g_ws_ctx.send_buffer_len = 0;
    g_ws_ctx.recv_buffer_len = 0;
    
    #ifndef TESTING
    // Create libwebsockets context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = NULL;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    
    g_ws_ctx.ws_context = lws_create_context(&info);
    
    if (g_ws_ctx.ws_context == NULL) {
        logger_error("Failed to create WebSocket context");
        return -1;
    }
    #else
    g_ws_ctx.ws_context = (void*)0x5678;  // Mock context
    #endif
    
    g_ws_ctx.initialized = true;
    
    #ifndef TESTING
    logger_info("WebSocket client initialized: %s:%d", server_host, server_port);
    #endif
    
    return 0;
}

void ws_client_set_callbacks(ws_connected_callback_t on_connected,
                             ws_disconnected_callback_t on_disconnected,
                             ws_message_callback_t on_message,
                             ws_error_callback_t on_error,
                             void *user_data) {
    g_ws_ctx.on_connected = on_connected;
    g_ws_ctx.on_disconnected = on_disconnected;
    g_ws_ctx.on_message = on_message;
    g_ws_ctx.on_error = on_error;
    g_ws_ctx.user_data = user_data;
}

int ws_client_connect(void) {
    if (!g_ws_ctx.initialized) {
        return -1;
    }
    
    if (g_ws_ctx.current_state == WS_STATE_CONNECTED ||
        g_ws_ctx.current_state == WS_STATE_CONNECTING) {
        return -1;  // Already connected or connecting
    }
    
    change_state(WS_STATE_CONNECTING);
    
    if (attempt_connect() < 0) {
        change_state(WS_STATE_ERROR);
        return -1;
    }
    
    g_ws_ctx.last_reconnect_time = get_current_time_ms();
    
    return 0;
}

int ws_client_send(const char *message) {
    if (!g_ws_ctx.initialized || message == NULL) {
        return -1;
    }
    
    if (g_ws_ctx.current_state != WS_STATE_CONNECTED) {
        return -1;  // Not connected
    }
    
    size_t len = strlen(message);
    if (len >= WS_MAX_MESSAGE_SIZE) {
        return -1;  // Message too large
    }
    
    // Copy to send buffer
    strncpy(g_ws_ctx.send_buffer, message, sizeof(g_ws_ctx.send_buffer) - 1);
    g_ws_ctx.send_buffer_len = len;
    
    #ifndef TESTING
    // Request callback to send
    if (g_ws_ctx.ws_connection != NULL) {
        lws_callback_on_writable(g_ws_ctx.ws_connection);
    }
    
    logger_debug("WebSocket message queued: %s", message);
    #endif
    
    return 0;
}

int ws_client_service(int timeout_ms) {
    if (!g_ws_ctx.initialized) {
        return -1;
    }
    
    #ifndef TESTING
    // Service libwebsockets
    if (g_ws_ctx.ws_context != NULL) {
        lws_service(g_ws_ctx.ws_context, timeout_ms);
    }
    #endif
    
    // Handle reconnection
    if (g_ws_ctx.current_state == WS_STATE_DISCONNECTED ||
        g_ws_ctx.current_state == WS_STATE_ERROR) {
        if (should_reconnect()) {
            #ifndef TESTING
            logger_info("Attempting WebSocket reconnection (attempt %d/%d)",
                    g_ws_ctx.reconnect_attempts + 1, WS_MAX_RECONNECT_ATTEMPTS);
            #endif
            
            g_ws_ctx.reconnect_attempts++;
            g_ws_ctx.last_reconnect_time = get_current_time_ms();
            
            if (ws_client_connect() < 0) {
                change_state(WS_STATE_ERROR);
            }
        }
    }
    
    // Handle heartbeat
    if (g_ws_ctx.current_state == WS_STATE_CONNECTED) {
        uint32_t current_time = get_current_time_ms();
        
        if (current_time - g_ws_ctx.last_ping_time >= g_ws_ctx.ping_interval) {
            send_ping();
        }
        
        // Check for pong timeout
        if (g_ws_ctx.waiting_for_pong &&
            current_time - g_ws_ctx.last_ping_time >= WS_PING_TIMEOUT_MS) {
            #ifndef TESTING
            logger_warning("WebSocket pong timeout, disconnecting");
            #endif
            ws_client_disconnect();
        }
    }
    
    return 0;
}

// 修正: 返回值改為 int
int ws_client_disconnect(void) {
    if (!g_ws_ctx.initialized) {
        return -1;
    }
    
    #ifndef TESTING
    if (g_ws_ctx.ws_connection != NULL) {
        lws_close_reason(g_ws_ctx.ws_connection, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        g_ws_ctx.ws_connection = NULL;
    }
    #else
    g_ws_ctx.ws_connection = NULL;
    #endif
    
    change_state(WS_STATE_DISCONNECTED);
    g_ws_ctx.auto_reconnect = false;
    
    return 0;
}

void ws_client_cleanup(void) {
    if (!g_ws_ctx.initialized) {
        return;
    }
    
    // Disconnect first
    ws_client_disconnect();
    
    #ifndef TESTING
    // Destroy context
    if (g_ws_ctx.ws_context != NULL) {
        lws_context_destroy(g_ws_ctx.ws_context);
        g_ws_ctx.ws_context = NULL;
    }
    #else
    g_ws_ctx.ws_context = NULL;
    #endif
    
    // Reset state
    g_ws_ctx.initialized = false;
    g_ws_ctx.current_state = WS_STATE_DISCONNECTED;
    g_ws_ctx.on_connected = NULL;
    g_ws_ctx.on_disconnected = NULL;
    g_ws_ctx.on_error = NULL;
    g_ws_ctx.on_message = NULL;
    g_ws_ctx.user_data = NULL;
    
    #ifndef TESTING
    logger_info("WebSocket client cleaned up");
    #endif
}

ws_state_t ws_client_get_state(void) {
    return g_ws_ctx.current_state;
}

void ws_client_set_auto_reconnect(bool enable) {
    g_ws_ctx.auto_reconnect = enable;
}

const char* ws_client_state_to_string(ws_state_t state) {
    switch (state) {
        case WS_STATE_DISCONNECTED: return "DISCONNECTED";
        case WS_STATE_CONNECTING:   return "CONNECTING";
        case WS_STATE_CONNECTED:    return "CONNECTED";
        case WS_STATE_DISCONNECTING: return "DISCONNECTING";
        case WS_STATE_ERROR:        return "ERROR";
        default:                    return "UNKNOWN";
    }
}

const char* ws_client_error_to_string(ws_error_t error) {
    switch (error) {
        case WS_ERROR_NONE:                 return "NO_ERROR";
        case WS_ERROR_INIT:                 return "INIT_FAILED";
        case WS_ERROR_CONNECT:              return "CONNECT_FAILED";
        case WS_ERROR_SEND:                 return "SEND_FAILED";
        case WS_ERROR_RECEIVE:              return "RECEIVE_FAILED";
        case WS_ERROR_TIMEOUT:              return "TIMEOUT";
        case WS_ERROR_PROTOCOL:             return "PROTOCOL_ERROR";
        case WS_ERROR_CLOSED:               return "CONNECTION_CLOSED";
        default:                            return "UNKNOWN_ERROR";
    }
}
