/**
 * @file button_handler.h
 * @brief Button Handler Module - GPIO button detection with debounce and press recognition
 * 
 * This module provides button event detection with:
 * - Debounce handling
 * - Short press detection (<2 seconds)
 * - Long press detection (>=2 seconds)
 * - Callback mechanism for event notification
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ButtonHandler Button Handler Module
 * @brief Button detection and event handling
 * @{
 */

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

/** Default debounce time in milliseconds */
#define BUTTON_DEFAULT_DEBOUNCE_MS    50

/** Long press threshold in milliseconds */
#define BUTTON_LONG_PRESS_THRESHOLD_MS 2000

/** Button polling interval in milliseconds */
#define BUTTON_POLL_INTERVAL_MS       10

/* ============================================================
 *  Type Definitions
 * ============================================================ */

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_NONE = 0,          /**< No event */
    BUTTON_EVENT_SHORT_PRESS,       /**< Short press detected (<2s) */
    BUTTON_EVENT_LONG_PRESS,        /**< Long press detected (>=2s) */
} button_event_t;

/**
 * @brief Button state for internal state machine
 */
typedef enum {
    BUTTON_STATE_IDLE = 0,          /**< Button not pressed */
    BUTTON_STATE_DEBOUNCING,        /**< Waiting for debounce */
    BUTTON_STATE_PRESSED,           /**< Button pressed, measuring duration */
    BUTTON_STATE_LONG_DETECTED,     /**< Long press detected */
} button_state_t;

/**
 * @brief Button event callback function type
 * 
 * @param event The button event that occurred
 * @param user_data User-provided data pointer
 */
typedef void (*button_callback_t)(button_event_t event, void *user_data);

/**
 * @brief Button handler configuration
 */
typedef struct {
    int gpio_pin;                   /**< GPIO pin number */
    int debounce_ms;                /**< Debounce time in milliseconds */
    int long_press_threshold_ms;    /**< Long press threshold in milliseconds */
    button_callback_t callback;     /**< Event callback function */
    void *user_data;                /**< User data for callback */
} button_config_t;

/* ============================================================
 *  Public Function Declarations
 * ============================================================ */

/**
 * @brief Initialize the button handler
 * 
 * This function initializes the button handler with the specified GPIO pin
 * and debounce time. The GPIO pin will be configured as input with pull-up.
 * 
 * @param pin GPIO pin number for the button
 * @param debounce_ms Debounce time in milliseconds (0 for default)
 * @return 0 on success, negative error code on failure
 * 
 * @note Call this function before any other button handler functions
 * @note Default debounce time is 50ms if debounce_ms is 0
 */
int button_handler_init(int pin, int debounce_ms);

/**
 * @brief Set the button event callback
 * 
 * Register a callback function to be called when button events occur.
 * 
 * @param callback Callback function pointer
 * @param user_data User data to pass to callback (can be NULL)
 * 
 * @note Only one callback can be registered at a time
 * @note Pass NULL to callback to unregister
 */
void button_handler_set_callback(button_callback_t callback, void *user_data);

/**
 * @brief Set long press threshold
 * 
 * Configure the time threshold for long press detection.
 * 
 * @param threshold_ms Threshold time in milliseconds
 * @return 0 on success, negative error code on failure
 */
int button_handler_set_long_press_threshold(int threshold_ms);

/**
 * @brief Process button events (non-blocking)
 * 
 * This function should be called regularly (e.g., every 10ms) from the
 * main event loop. It checks the button state and triggers callbacks
 * when events are detected.
 * 
 * @return 0 on success, negative error code on failure
 * 
 * @note This function is non-blocking and should be called frequently
 * @note Callback functions are called from within this function
 */
int button_handler_process(void);

/**
 * @brief Run the button handler loop (blocking)
 * 
 * This function enters a blocking loop that continuously processes button
 * events. It will run until button_handler_stop() is called from a callback
 * or another thread.
 * 
 * @return 0 on normal exit, negative error code on failure
 * 
 * @note This function blocks until stopped
 * @note Use button_handler_process() for integration with existing event loops
 */
int button_handler_run(void);

/**
 * @brief Stop the button handler loop
 * 
 * Signal the blocking loop in button_handler_run() to exit.
 * Safe to call from callback functions or other threads.
 */
void button_handler_stop(void);

/**
 * @brief Get current button state (for debugging)
 * 
 * @return Current button state
 */
button_state_t button_handler_get_state(void);

/**
 * @brief Check if button is currently pressed
 * 
 * @return true if button is pressed, false otherwise
 */
bool button_handler_is_pressed(void);

/**
 * @brief Clean up button handler resources
 * 
 * Release all resources and reset the button handler state.
 * After calling this function, button_handler_init() must be called
 * again before using the button handler.
 */
void button_handler_cleanup(void);

/**
 * @brief Get the string name of a button event
 * 
 * @param event The button event
 * @return String representation of the event
 */
const char* button_event_to_string(button_event_t event);

/**
 * @brief Get the string name of a button state
 * 
 * @param state The button state
 * @return String representation of the state
 */
const char* button_state_to_string(button_state_t state);

/** @} */ // end of ButtonHandler group

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_HANDLER_H */
