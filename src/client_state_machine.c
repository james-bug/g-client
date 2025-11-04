/**
 * @file client_state_machine.c
 * @brief Client State Machine Implementation
 * 
 * This module orchestrates the entire client behavior by coordinating:
 * - Button handler for user input
 * - VPN controller for VPN connection
 * - WebSocket client for server communication
 * - LED controller for status indication
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

// POSIX headers for time and sleep functions
#define _POSIX_C_SOURCE 200112L

#include "client_state_machine.h"
#include "button_handler.h"
#include "vpn_controller.h"
#include "websocket_client.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/led_controller.h>
    #include <gaming/logger.h>
  #else
    #include "../../gaming-core/src/led_controller.h"
    #include "../../gaming-core/src/logger.h"
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

/* ============================================================
 *  Internal Structures
 * ============================================================ */

/**
 * @brief Client context implementation
 */
struct client_context_t {
    bool initialized;
    client_state_t current_state;
    client_state_t previous_state;
    
    // Configuration
    client_config_t config;
    
    // Current PS5 status
    ps5_status_t ps5_status;
    
    // Callbacks
    client_state_callback_t state_callback;
    client_error_callback_t error_callback;
    void *user_data;
    
    // Statistics
    client_stats_t stats;
    
    // Timeout tracking
    uint32_t state_enter_time;
    uint32_t current_timeout;
    
    // Error handling
    client_error_t last_error;
    int error_count;
    bool in_error_recovery;
    
    // LED update tracking
    bool led_update_done;
    uint32_t led_update_start_time;
};

/* ============================================================
 *  Forward Declarations
 * ============================================================ */

static void change_state(client_context_t *ctx, client_state_t new_state);
static void report_error(client_context_t *ctx, client_error_t error, const char *message);
static void update_led_for_state(client_context_t *ctx, client_state_t state);
static void update_led_for_ps5_status(client_context_t *ctx, ps5_status_t status);

#ifndef TESTING
static void on_button_event(button_event_t event, void *user_data);
#endif
static void on_vpn_state_change(vpn_state_t old_state, vpn_state_t new_state, void *user_data);
static void on_ws_message(const char *message, size_t length, void *user_data);
static void on_ws_connected(void *user_data);
static void on_ws_disconnected(const char *reason, void *user_data);
static void on_ws_error(ws_error_t error, const char *message, void *user_data);

static void handle_idle_state(client_context_t *ctx);
static void handle_vpn_connecting_state(client_context_t *ctx);
static void handle_vpn_connected_state(client_context_t *ctx);
static void handle_ws_connecting_state(client_context_t *ctx);
static void handle_querying_ps5_state(client_context_t *ctx);
static void handle_led_update_state(client_context_t *ctx);
static void handle_waiting_state(client_context_t *ctx);
static void handle_error_state(client_context_t *ctx);
static void handle_cleanup_state(client_context_t *ctx);

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
 * @brief Check if state timeout occurred
 */
static bool is_state_timeout(client_context_t *ctx) {
    if (ctx->current_timeout == 0) {
        return false;
    }
    
    uint32_t current_time = get_current_time_ms();
    return (current_time - ctx->state_enter_time) >= ctx->current_timeout;
}

/**
 * @brief Change state and trigger callback
 */
static void change_state(client_context_t *ctx, client_state_t new_state) {
    if (ctx->current_state == new_state) {
        return;
    }
    
    ctx->previous_state = ctx->current_state;
    ctx->current_state = new_state;
    ctx->state_enter_time = get_current_time_ms();
    
    #ifndef TESTING
    logger_info("Client state changed: %s -> %s",
             client_state_to_string(ctx->previous_state),
             client_state_to_string(new_state));
    #endif
    
    // Trigger callback
    if (ctx->state_callback != NULL) {
        ctx->state_callback(ctx->previous_state, new_state, ctx->user_data);
    }
}

/**
 * @brief Report error
 */
static void report_error(client_context_t *ctx, client_error_t error, const char *message) {
    ctx->last_error = error;
    ctx->error_count++;
    ctx->stats.error_count++;
    
    #ifndef TESTING
    logger_error("Client error: %s - %s", client_error_to_string(error), message);
    #endif
    
    if (ctx->error_callback != NULL) {
        ctx->error_callback(error, message, ctx->user_data);
    }
}

/**
 * @brief Update LED based on state
 */
static void update_led_for_state(client_context_t *ctx, client_state_t state) {
    #ifndef TESTING
    switch (state) {
        case CLIENT_STATE_IDLE:
            // LED off in idle
            led_off();
            break;
            
        case CLIENT_STATE_VPN_CONNECTING:
            // Yellow blinking
            led_set_color(255, 255, 0);
            led_blink(LED_COLOR_BLUE, -1, 500);
            break;
            
        case CLIENT_STATE_VPN_CONNECTED:
        case CLIENT_STATE_WS_CONNECTING:
            // Yellow solid
            led_set_color(255, 255, 0);
            led_off();
            break;
            
        case CLIENT_STATE_QUERYING_PS5:
            // Blue blinking
            led_set_color(0, 0, 255);
            led_blink(LED_COLOR_RED, -1, 250);
            break;
            
        case CLIENT_STATE_ERROR:
            // Red blinking
            led_set_color(255, 0, 0);
            led_blink(LED_COLOR_YELLOW, -1, 200);
            break;
            
        default:
            break;
    }
    #endif
}

/**
 * @brief Update LED based on PS5 status
 */
static void update_led_for_ps5_status(client_context_t *ctx, ps5_status_t status) {
    #ifndef TESTING
    ctx->led_update_start_time = get_current_time_ms();
    ctx->led_update_done = false;
    
    switch (status) {
        case PS5_STATUS_ON:
            // Green
            led_set_color(0, 255, 0);
            led_off();
            break;
            
        case PS5_STATUS_STANDBY:
            // Orange
            led_set_color(255, 165, 0);
            led_off();
            break;
            
        case PS5_STATUS_OFF:
            // Red
            led_set_color(255, 0, 0);
            led_off();
            break;
            
        case PS5_STATUS_UNKNOWN:
        default:
            // Purple (unknown)
            led_set_color(128, 0, 128);
            led_off();
            break;
    }
    
    logger_info("LED updated for PS5 status: %s", ps5_status_to_string(status));
    #endif
}

/**
 * @brief Button event callback
 */
#ifndef TESTING
static void on_button_event(button_event_t event, void *user_data) {
    client_context_t *ctx = (client_context_t *)user_data;
    
    if (ctx == NULL) {
        return;
    }
    
    logger_info("Button event: %s", button_event_to_string(event));
    
    ctx->stats.button_press_count++;
    
    // Only handle short press in idle state
    if (event == BUTTON_EVENT_SHORT_PRESS && ctx->current_state == CLIENT_STATE_IDLE) {
        change_state(ctx, CLIENT_STATE_VPN_CONNECTING);
    }
}
#endif

/**
 * @brief VPN state change callback
 */
static void on_vpn_state_change(vpn_state_t old_state, vpn_state_t new_state, void *user_data) {
    client_context_t *ctx = (client_context_t *)user_data;
    
    if (ctx == NULL) {
        return;
    }
    
    #ifndef TESTING
    logger_info("VPN state changed: %s -> %s", 
             vpn_controller_state_to_string(old_state), vpn_controller_state_to_string(new_state));
    #endif
    
    if (new_state == VPN_STATE_CONNECTED) {
        ctx->stats.vpn_success_count++;
    }
}

/**
 * @brief WebSocket message callback
 */
static void on_ws_message(const char *message, size_t length, void *user_data) {
    client_context_t *ctx = (client_context_t *)user_data;
    
    if (ctx == NULL || message == NULL) {
        return;
    }
    
    #ifndef TESTING
    logger_debug("WebSocket message received (%zu bytes)", length);
    #endif
    
    // Parse PS5 status from message
    // Simple parsing - in production use proper JSON library
    if (strstr(message, "\"status\":\"on\"")) {
        ctx->ps5_status = PS5_STATUS_ON;
        ctx->stats.successful_queries++;
    } else if (strstr(message, "\"status\":\"standby\"")) {
        ctx->ps5_status = PS5_STATUS_STANDBY;
        ctx->stats.successful_queries++;
    } else if (strstr(message, "\"status\":\"off\"")) {
        ctx->ps5_status = PS5_STATUS_OFF;
        ctx->stats.successful_queries++;
    } else {
        ctx->ps5_status = PS5_STATUS_UNKNOWN;
        ctx->stats.failed_queries++;
    }
    
    ctx->stats.last_query_time = time(NULL);
    
    // Transition to LED update state
    if (ctx->current_state == CLIENT_STATE_QUERYING_PS5) {
        change_state(ctx, CLIENT_STATE_LED_UPDATE);
    }
}

/**
 * @brief WebSocket connected callback
 */
static void on_ws_connected(void *user_data) {
    client_context_t *ctx = (client_context_t *)user_data;
    
    if (ctx == NULL) {
        return;
    }
    
    #ifndef TESTING
    logger_info("WebSocket connected");
    #endif
    
    // Transition to querying state if in connecting state
    if (ctx->current_state == CLIENT_STATE_WS_CONNECTING) {
        change_state(ctx, CLIENT_STATE_QUERYING_PS5);
    }
}

/**
 * @brief WebSocket disconnected callback
 */
static void on_ws_disconnected(const char *reason, void *user_data) {
    #ifndef TESTING
    logger_warning("WebSocket disconnected: %s", reason ? reason : "unknown");
    #endif
    (void)user_data;  // Unused in current implementation
}

/**
 * @brief WebSocket error callback
 */
static void on_ws_error(ws_error_t error, const char *message, void *user_data) {
    client_context_t *ctx = (client_context_t *)user_data;
    
    if (ctx == NULL) {
        return;
    }
    
    #ifndef TESTING
    logger_error("WebSocket error: %s - %s", 
              ws_client_error_to_string(error), 
              message ? message : "unknown");
    #endif
    
    if (ctx->current_state == CLIENT_STATE_WS_CONNECTING ||
        ctx->current_state == CLIENT_STATE_QUERYING_PS5) {
        report_error(ctx, CLIENT_ERROR_WS_FAILED, "WebSocket error");
        change_state(ctx, CLIENT_STATE_ERROR);
    }
}

/* ============================================================
 *  State Handler Functions
 * ============================================================ */

static void handle_idle_state(client_context_t *ctx) {
    // Just wait for button press
    // Button callback will trigger state change
    (void)ctx;  // Unused in current implementation
}

static void handle_vpn_connecting_state(client_context_t *ctx) {
    vpn_state_t vpn_state = vpn_controller_get_state();
    
    if (vpn_state == VPN_STATE_CONNECTED) {
        change_state(ctx, CLIENT_STATE_VPN_CONNECTED);
        ctx->stats.vpn_connect_count++;
    } else if (vpn_state == VPN_STATE_ERROR) {
        report_error(ctx, CLIENT_ERROR_VPN_FAILED, "VPN connection failed");
        change_state(ctx, CLIENT_STATE_ERROR);
    } else if (is_state_timeout(ctx)) {
        report_error(ctx, CLIENT_ERROR_VPN_TIMEOUT, "VPN connection timeout");
        change_state(ctx, CLIENT_STATE_ERROR);
    }
}

static void handle_vpn_connected_state(client_context_t *ctx) {
    // Start WebSocket connection
    if (ws_client_get_state() != WS_STATE_CONNECTED &&
        ws_client_get_state() != WS_STATE_CONNECTING) {
        
        if (ws_client_connect() == 0) {
            change_state(ctx, CLIENT_STATE_WS_CONNECTING);
        } else {
            report_error(ctx, CLIENT_ERROR_WS_FAILED, "Failed to start WebSocket connection");
            change_state(ctx, CLIENT_STATE_ERROR);
        }
    }
}

static void handle_ws_connecting_state(client_context_t *ctx) {
    ws_state_t ws_state = ws_client_get_state();
    
    if (ws_state == WS_STATE_CONNECTED) {
        // Will be handled by callback
    } else if (ws_state == WS_STATE_ERROR) {
        report_error(ctx, CLIENT_ERROR_WS_FAILED, "WebSocket connection failed");
        change_state(ctx, CLIENT_STATE_ERROR);
    } else if (is_state_timeout(ctx)) {
        report_error(ctx, CLIENT_ERROR_WS_TIMEOUT, "WebSocket connection timeout");
        change_state(ctx, CLIENT_STATE_ERROR);
    }
}

static void handle_querying_ps5_state(client_context_t *ctx) {
    // Send PS5 query if not already sent
    static bool query_sent = false;
    
    if (!query_sent) {
        if (ws_client_send("{\"type\":\"query_ps5\"}") == 0) {
            query_sent = true;
            #ifndef TESTING
            logger_info("PS5 query sent");
            #endif
        } else {
            report_error(ctx, CLIENT_ERROR_PS5_FAILED, "Failed to send PS5 query");
            change_state(ctx, CLIENT_STATE_ERROR);
            query_sent = false;
            return;
        }
    }
    
    // Check for timeout
    if (is_state_timeout(ctx)) {
        report_error(ctx, CLIENT_ERROR_PS5_TIMEOUT, "PS5 query timeout");
        change_state(ctx, CLIENT_STATE_ERROR);
        query_sent = false;
        ctx->stats.failed_queries++;
    }
    
    // Response will be handled by callback
}

static void handle_led_update_state(client_context_t *ctx) {
    if (!ctx->led_update_done) {
        update_led_for_ps5_status(ctx, ctx->ps5_status);
        ctx->led_update_done = true;
    }
    
    // Wait for LED update duration
    uint32_t current_time = get_current_time_ms();
    if (current_time - ctx->led_update_start_time >= CLIENT_LED_UPDATE_DURATION_S * 1000) {
        change_state(ctx, CLIENT_STATE_WAITING);
    }
}

static void handle_waiting_state(client_context_t *ctx) {
    // Disconnect WebSocket
    ws_client_disconnect();
    
    // Disconnect VPN
    vpn_controller_disconnect();
    
    // Return to idle
    change_state(ctx, CLIENT_STATE_CLEANUP);
}

static void handle_error_state(client_context_t *ctx) {
    // Update LED to show error
    update_led_for_state(ctx, CLIENT_STATE_ERROR);
    
    // Check if should retry
    if (ctx->error_count < ctx->config.max_retry_attempts && ctx->config.auto_retry) {
        // Wait a bit before retry
        struct timespec sleep_time = {
            .tv_sec = CLIENT_RETRY_INTERVAL_S,
            .tv_nsec = 0
        };
        nanosleep(&sleep_time, NULL);
        
        #ifndef TESTING
        logger_info("Retrying after error (attempt %d/%d)", 
                ctx->error_count, ctx->config.max_retry_attempts);
        #endif
        
        change_state(ctx, CLIENT_STATE_CLEANUP);
    } else {
        // Max retries exceeded or auto retry disabled
        report_error(ctx, CLIENT_ERROR_MAX_RETRIES, "Maximum retry attempts exceeded");
        
        // Wait before cleanup
        struct timespec sleep_time = {
            .tv_sec = 5,
            .tv_nsec = 0
        };
        nanosleep(&sleep_time, NULL);
        
        change_state(ctx, CLIENT_STATE_CLEANUP);
    }
}

static void handle_cleanup_state(client_context_t *ctx) {
    // Ensure everything is disconnected
    ws_client_disconnect();
    vpn_controller_disconnect();
    
    // Turn off LED
    #ifndef TESTING
    led_off();
    led_off();
    #endif
    
    // Reset error count if successful
    if (ctx->last_error == CLIENT_ERROR_NONE) {
        ctx->error_count = 0;
    }
    
    // Return to idle
    change_state(ctx, CLIENT_STATE_IDLE);
}

/* ============================================================
 *  Public API Implementation
 * ============================================================ */

client_context_t* client_sm_create(const client_config_t *config) {
    if (config == NULL) {
        return NULL;
    }
    
    client_context_t *ctx = (client_context_t *)malloc(sizeof(client_context_t));
    if (ctx == NULL) {
        return NULL;
    }
    
    memset(ctx, 0, sizeof(client_context_t));
    memcpy(&ctx->config, config, sizeof(client_config_t));
    
    ctx->current_state = CLIENT_STATE_IDLE;
    ctx->previous_state = CLIENT_STATE_IDLE;
    ctx->ps5_status = PS5_STATUS_UNKNOWN;
    ctx->initialized = false;
    
    return ctx;
}

int client_sm_init(client_context_t *ctx) {
    if (ctx == NULL) {
        return -1;
    }
    
    if (ctx->initialized) {
        return -1;  // Already initialized
    }
    
    // Initialize button handler
    #ifndef TESTING
    if (button_handler_init(ctx->config.button_pin, ctx->config.button_debounce_ms) < 0) {
        logger_error("Failed to initialize button handler");
        return -1;
    }
    button_handler_set_callback(on_button_event, ctx);
    #endif
    
    // Initialize VPN controller
    if (vpn_controller_init(ctx->config.vpn_socket_path) < 0) {
        #ifndef TESTING
        logger_error("Failed to initialize VPN controller");
        button_handler_cleanup();
        #endif
        return -1;
    }
    vpn_controller_set_callback(on_vpn_state_change, ctx);
    
    // Initialize WebSocket client
    if (ws_client_init(ctx->config.ws_server_host, ctx->config.ws_server_port) < 0) {
        #ifndef TESTING
        logger_error("Failed to initialize WebSocket client");
        button_handler_cleanup();
        #endif
        vpn_controller_cleanup();
        return -1;
    }
    // 🔧 FIXED: Correct parameter order for ws_client_set_callbacks
    ws_client_set_callbacks(on_ws_connected, on_ws_disconnected, on_ws_message, on_ws_error, ctx);
    
    ctx->initialized = true;
    ctx->current_state = CLIENT_STATE_IDLE;
    ctx->state_enter_time = get_current_time_ms();
    
    #ifndef TESTING
    logger_info("Client state machine initialized");
    #endif
    
    return 0;
}

void client_sm_set_state_callback(client_context_t *ctx,
                                  client_state_callback_t callback,
                                  void *user_data) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->state_callback = callback;
    ctx->user_data = user_data;
}

void client_sm_set_error_callback(client_context_t *ctx,
                                  client_error_callback_t callback,
                                  void *user_data) {
    if (ctx == NULL) {
        return;
    }
    
    ctx->error_callback = callback;
    // Note: user_data is set by state_callback, shared between both
}

int client_sm_update(client_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }
    
    // Process sub-modules
    #ifndef TESTING
    button_handler_process();
    #endif
    vpn_controller_process(10);      // 🔧 FIXED: Added timeout parameter
    ws_client_service(10);           // 🔧 FIXED: Changed from ws_client_process to ws_client_service
    
    // Set timeout based on state
    switch (ctx->current_state) {
        case CLIENT_STATE_VPN_CONNECTING:
            ctx->current_timeout = CLIENT_VPN_CONNECT_TIMEOUT_S * 1000;
            break;
        case CLIENT_STATE_WS_CONNECTING:
            ctx->current_timeout = CLIENT_WS_CONNECT_TIMEOUT_S * 1000;
            break;
        case CLIENT_STATE_QUERYING_PS5:
            ctx->current_timeout = CLIENT_PS5_QUERY_TIMEOUT_S * 1000;
            break;
        default:
            ctx->current_timeout = 0;
            break;
    }
    
    // Update LED for current state
    if (ctx->current_state != ctx->previous_state) {
        update_led_for_state(ctx, ctx->current_state);
    }
    
    // Handle current state
    switch (ctx->current_state) {
        case CLIENT_STATE_IDLE:
            handle_idle_state(ctx);
            break;
        case CLIENT_STATE_VPN_CONNECTING:
            handle_vpn_connecting_state(ctx);
            break;
        case CLIENT_STATE_VPN_CONNECTED:
            handle_vpn_connected_state(ctx);
            break;
        case CLIENT_STATE_WS_CONNECTING:
            handle_ws_connecting_state(ctx);
            break;
        case CLIENT_STATE_QUERYING_PS5:
            handle_querying_ps5_state(ctx);
            break;
        case CLIENT_STATE_LED_UPDATE:
            handle_led_update_state(ctx);
            break;
        case CLIENT_STATE_WAITING:
            handle_waiting_state(ctx);
            break;
        case CLIENT_STATE_ERROR:
            handle_error_state(ctx);
            break;
        case CLIENT_STATE_CLEANUP:
            handle_cleanup_state(ctx);
            break;
    }
    
    return 0;  // 🔧 FIXED: Return int instead of void
}

client_state_t client_sm_get_state(const client_context_t *ctx) {  // 🔧 FIXED: Added const
    if (ctx == NULL) {
        return CLIENT_STATE_IDLE;
    }
    return ctx->current_state;
}

ps5_status_t client_sm_get_ps5_status(const client_context_t *ctx) {  // 🔧 FIXED: Added const
    if (ctx == NULL) {
        return PS5_STATUS_UNKNOWN;
    }
    return ctx->ps5_status;
}

int client_sm_get_stats(const client_context_t *ctx, client_stats_t *stats) {  // 🔧 FIXED: Return int and added const
    if (ctx == NULL || stats == NULL) {
        return -1;
    }
    memcpy(stats, &ctx->stats, sizeof(client_stats_t));
    return 0;
}

void client_sm_cleanup(client_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    if (!ctx->initialized) {
        return;
    }
    
    // Cleanup all modules
    #ifndef TESTING
    button_handler_cleanup();
    #endif
    vpn_controller_cleanup();
    ws_client_cleanup();
    
    ctx->initialized = false;
    
    #ifndef TESTING
    logger_info("Client state machine cleaned up");
    #endif
}

void client_sm_destroy(client_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    client_sm_cleanup(ctx);
    free(ctx);
}

const char* client_state_to_string(client_state_t state) {
    switch (state) {
        case CLIENT_STATE_IDLE:           return "IDLE";
        case CLIENT_STATE_VPN_CONNECTING: return "VPN_CONNECTING";
        case CLIENT_STATE_VPN_CONNECTED:  return "VPN_CONNECTED";
        case CLIENT_STATE_WS_CONNECTING:  return "WS_CONNECTING";
        case CLIENT_STATE_QUERYING_PS5:   return "QUERYING_PS5";
        case CLIENT_STATE_LED_UPDATE:     return "LED_UPDATE";
        case CLIENT_STATE_WAITING:        return "WAITING";
        case CLIENT_STATE_ERROR:          return "ERROR";
        case CLIENT_STATE_CLEANUP:        return "CLEANUP";
        default:                          return "UNKNOWN";
    }
}

const char* client_error_to_string(client_error_t error) {
    switch (error) {
        case CLIENT_ERROR_NONE:           return "NO_ERROR";
        case CLIENT_ERROR_VPN_TIMEOUT:    return "VPN_TIMEOUT";
        case CLIENT_ERROR_VPN_FAILED:     return "VPN_FAILED";
        case CLIENT_ERROR_WS_TIMEOUT:     return "WS_TIMEOUT";
        case CLIENT_ERROR_WS_FAILED:      return "WS_FAILED";
        case CLIENT_ERROR_PS5_TIMEOUT:    return "PS5_TIMEOUT";
        case CLIENT_ERROR_PS5_FAILED:     return "PS5_FAILED";
        case CLIENT_ERROR_MAX_RETRIES:    return "MAX_RETRIES";
        default:                          return "UNKNOWN_ERROR";
    }
}

const char* ps5_status_to_string(ps5_status_t status) {
    switch (status) {
        case PS5_STATUS_UNKNOWN:  return "UNKNOWN";
        case PS5_STATUS_OFF:      return "OFF";
        case PS5_STATUS_STANDBY:  return "STANDBY";
        case PS5_STATUS_ON:       return "ON";
        default:                  return "INVALID";
    }
}
