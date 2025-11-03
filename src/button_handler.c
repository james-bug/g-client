/**
 * @file button_handler.c
 * @brief Button Handler Implementation - GPIO button detection with debounce
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

// POSIX headers for time and sleep functions
#define _POSIX_C_SOURCE 199309L

#include "button_handler.h"

// Standard C library headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <time.h>
#include <unistd.h>

// gaming-core dependencies
#ifndef TESTING
  #ifdef OPENWRT_BUILD
    // OpenWrt build - gaming-core installed in staging dir
    #include <gaming/hal_interface.h>
    #include <gaming/gpio_lib.h>
  #else
    // Development build - relative path to gaming-core
    #include "../../gaming-core/src/hal_interface.h"
    #include "../../gaming-core/src/gpio_lib.h"
  #endif
#endif

/* ============================================================
 *  Internal Structures
 * ============================================================ */

/**
 * @brief Button context - internal state management
 */
typedef struct {
    // Configuration
    int gpio_pin;
    int debounce_ms;
    int long_press_threshold_ms;
    
    // State
    button_state_t current_state;
    bool initialized;
    bool running;
    
    // Callback
    button_callback_t callback;
    void *user_data;
    
    // Timing
    struct timespec press_start_time;
    struct timespec last_state_change_time;
    
    // Debounce
    int last_raw_value;
    int stable_value;
    int stable_count;
    bool long_press_triggered;
    
} button_context_t;

/* ============================================================
 *  Static Variables
 * ============================================================ */

static button_context_t g_button_ctx = {0};

/* ============================================================
 *  Internal Helper Functions
 * ============================================================ */

/**
 * @brief Calculate time difference in milliseconds
 */
static long timespec_diff_ms(struct timespec *start, struct timespec *end) {
    long sec_diff = end->tv_sec - start->tv_sec;
    long nsec_diff = end->tv_nsec - start->tv_nsec;
    return sec_diff * 1000 + nsec_diff / 1000000;
}

/**
 * @brief Get current monotonic time
 */
static void get_current_time(struct timespec *ts) {
    clock_gettime(CLOCK_MONOTONIC, ts);
}

/**
 * @brief Trigger button event callback
 */
static void trigger_event(button_event_t event) {
    if (g_button_ctx.callback) {
        g_button_ctx.callback(event, g_button_ctx.user_data);
    }
}

/**
 * @brief Change state and update timestamp
 */
static void change_state(button_state_t new_state) {
    if (g_button_ctx.current_state != new_state) {
        g_button_ctx.current_state = new_state;
        get_current_time(&g_button_ctx.last_state_change_time);
    }
}

/* ============================================================
 *  State Machine Logic
 * ============================================================ */

/**
 * @brief Update button state machine
 * 
 * This function implements the core button detection logic:
 * - Debounce filtering
 * - Short/Long press detection
 * - State transitions
 */
static void update_state_machine(void) {
    // Read current button state from GPIO
#ifdef TESTING
    // In test mode, use placeholder (will be mocked)
    int current_value = 0;
#else
    // In real mode, use gaming-core GPIO library
    int current_value = gpio_lib_read(g_button_ctx.gpio_pin);
#endif
    
    if (current_value < 0) {
        // GPIO read error - stay in current state
        return;
    }
    
    struct timespec now;
    get_current_time(&now);
    
    // Button is active LOW (pressed = 0, released = 1)
    bool button_pressed = (current_value == 0);
    
    switch (g_button_ctx.current_state) {
        case BUTTON_STATE_IDLE:
            if (button_pressed) {
                // Button just pressed - start debouncing
                change_state(BUTTON_STATE_DEBOUNCING);
                g_button_ctx.stable_count = 0;
                g_button_ctx.long_press_triggered = false;
            }
            break;
            
        case BUTTON_STATE_DEBOUNCING: {
            // Check if button state is stable
            if (button_pressed) {
                g_button_ctx.stable_count++;
                
                // Debounce period = debounce_ms / BUTTON_POLL_INTERVAL_MS samples
                int required_stable = g_button_ctx.debounce_ms / BUTTON_POLL_INTERVAL_MS;
                if (required_stable < 1) required_stable = 1;
                
                if (g_button_ctx.stable_count >= required_stable) {
                    // Button is stably pressed - transition to PRESSED
                    change_state(BUTTON_STATE_PRESSED);
                    g_button_ctx.press_start_time = now;
                }
            } else {
                // Button released during debounce - back to IDLE
                change_state(BUTTON_STATE_IDLE);
            }
            break;
        }
            
        case BUTTON_STATE_PRESSED: {
            if (!button_pressed) {
                // Button released - it was a short press
                if (!g_button_ctx.long_press_triggered) {
                    trigger_event(BUTTON_EVENT_SHORT_PRESS);
                }
                change_state(BUTTON_STATE_IDLE);
            } else {
                // Button still pressed - check for long press
                long press_duration = timespec_diff_ms(&g_button_ctx.press_start_time, &now);
                
                if (press_duration >= g_button_ctx.long_press_threshold_ms && 
                    !g_button_ctx.long_press_triggered) {
                    // Long press detected
                    g_button_ctx.long_press_triggered = true;
                    trigger_event(BUTTON_EVENT_LONG_PRESS);
                    change_state(BUTTON_STATE_LONG_DETECTED);
                }
            }
            break;
        }
            
        case BUTTON_STATE_LONG_DETECTED:
            if (!button_pressed) {
                // Button released after long press
                change_state(BUTTON_STATE_IDLE);
            }
            // Stay in this state while button is held
            break;
            
        default:
            // Invalid state - reset
            change_state(BUTTON_STATE_IDLE);
            break;
    }
}

/* ============================================================
 *  Public Function Implementations
 * ============================================================ */

int button_handler_init(int pin, int debounce_ms) {
    if (g_button_ctx.initialized) {
        fprintf(stderr, "[Button] Already initialized\n");
        return -1;
    }
    
    if (pin < 0) {
        fprintf(stderr, "[Button] Invalid GPIO pin: %d\n", pin);
        return -1;
    }
    
    // Initialize context
    memset(&g_button_ctx, 0, sizeof(button_context_t));
    
    g_button_ctx.gpio_pin = pin;
    g_button_ctx.debounce_ms = (debounce_ms > 0) ? debounce_ms : BUTTON_DEFAULT_DEBOUNCE_MS;
    g_button_ctx.long_press_threshold_ms = BUTTON_LONG_PRESS_THRESHOLD_MS;
    g_button_ctx.current_state = BUTTON_STATE_IDLE;
    g_button_ctx.initialized = true;
    g_button_ctx.running = false;
    
#ifndef TESTING
    // Initialize GPIO as input (only in real hardware mode)
    int result = gpio_lib_init_input(pin);
    if (result != 0) {
        fprintf(stderr, "[Button] Failed to initialize GPIO%d: %d\n", pin, result);
        g_button_ctx.initialized = false;
        return -1;
    }
#endif
    
    fprintf(stdout, "[Button] Initialized on GPIO%d (debounce=%dms)\n", 
            pin, g_button_ctx.debounce_ms);
    
    return 0;
}

void button_handler_set_callback(button_callback_t callback, void *user_data) {
    g_button_ctx.callback = callback;
    g_button_ctx.user_data = user_data;
}

int button_handler_set_long_press_threshold(int threshold_ms) {
    if (!g_button_ctx.initialized) {
        fprintf(stderr, "[Button] Not initialized\n");
        return -1;
    }
    
    if (threshold_ms < 100) {
        fprintf(stderr, "[Button] Threshold too small: %d ms\n", threshold_ms);
        return -1;
    }
    
    g_button_ctx.long_press_threshold_ms = threshold_ms;
    return 0;
}

int button_handler_process(void) {
    if (!g_button_ctx.initialized) {
        return -1;
    }
    
    update_state_machine();
    return 0;
}

int button_handler_run(void) {
    if (!g_button_ctx.initialized) {
        fprintf(stderr, "[Button] Not initialized\n");
        return -1;
    }
    
    g_button_ctx.running = true;
    
    fprintf(stdout, "[Button] Starting event loop...\n");
    
    // Sleep duration for polling
    struct timespec sleep_time = {
        .tv_sec = 0,
        .tv_nsec = BUTTON_POLL_INTERVAL_MS * 1000000  // ms to ns
    };
    
    while (g_button_ctx.running) {
        button_handler_process();
        nanosleep(&sleep_time, NULL);
    }
    
    fprintf(stdout, "[Button] Event loop stopped\n");
    return 0;
}

void button_handler_stop(void) {
    g_button_ctx.running = false;
}

button_state_t button_handler_get_state(void) {
    return g_button_ctx.current_state;
}

bool button_handler_is_pressed(void) {
    return (g_button_ctx.current_state == BUTTON_STATE_PRESSED ||
            g_button_ctx.current_state == BUTTON_STATE_LONG_DETECTED);
}

void button_handler_cleanup(void) {
    if (!g_button_ctx.initialized) {
        return;
    }
    
    g_button_ctx.running = false;
    
#ifndef TESTING
    // Cleanup GPIO (only in real hardware mode)
    gpio_lib_cleanup(g_button_ctx.gpio_pin);
#endif
    
    memset(&g_button_ctx, 0, sizeof(button_context_t));
    
    fprintf(stdout, "[Button] Cleaned up\n");
}

const char* button_event_to_string(button_event_t event) {
    switch (event) {
        case BUTTON_EVENT_NONE:        return "NONE";
        case BUTTON_EVENT_SHORT_PRESS: return "SHORT_PRESS";
        case BUTTON_EVENT_LONG_PRESS:  return "LONG_PRESS";
        default:                       return "UNKNOWN";
    }
}

const char* button_state_to_string(button_state_t state) {
    switch (state) {
        case BUTTON_STATE_IDLE:          return "IDLE";
        case BUTTON_STATE_DEBOUNCING:    return "DEBOUNCING";
        case BUTTON_STATE_PRESSED:       return "PRESSED";
        case BUTTON_STATE_LONG_DETECTED: return "LONG_DETECTED";
        default:                         return "UNKNOWN";
    }
}
