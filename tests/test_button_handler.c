/**
 * @file test_button_handler.c
 * @brief Unit tests for Button Handler module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 */

#include "unity.h"
#include "button_handler.h"

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

void setUp(void) {
    // Each test starts with clean state
}

void tearDown(void) {
    // Clean up after each test
    button_handler_cleanup();
}

/* ============================================================
 *  Test Group 1: Initialization Tests
 * ============================================================ */

void test_button_handler_init_should_succeed_with_valid_pin(void) {
    // Act
    int result = button_handler_init(17, 50);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(BUTTON_STATE_IDLE, button_handler_get_state());
}

void test_button_handler_init_should_fail_with_invalid_pin(void) {
    // Act
    int result = button_handler_init(-1, 50);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_button_handler_init_should_fail_when_already_initialized(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    int result = button_handler_init(17, 50);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_button_handler_init_should_use_default_debounce_when_zero(void) {
    // Act
    int result = button_handler_init(17, 0);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    // Debounce should be set to default (50ms)
    // We can't directly test this without exposing internal state
    // but we verify initialization succeeded
}

/* ============================================================
 *  Test Group 2: Callback Tests
 * ============================================================ */

static button_event_t g_received_event;
static void *g_received_user_data;
static int g_callback_count;

static void test_callback(button_event_t event, void *user_data) {
    g_received_event = event;
    g_received_user_data = user_data;
    g_callback_count++;
}

void test_button_handler_should_accept_callback(void) {
    // Arrange
    button_handler_init(17, 50);
    int user_data = 42;
    
    // Act
    button_handler_set_callback(test_callback, &user_data);
    
    // Assert
    // No error should occur
    TEST_PASS();
}

void test_button_handler_should_accept_null_callback(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    button_handler_set_callback(NULL, NULL);
    
    // Assert
    // Should not crash
    TEST_PASS();
}

/* ============================================================
 *  Test Group 3: State Tests
 * ============================================================ */

void test_button_handler_should_start_in_idle_state(void) {
    // Arrange & Act
    button_handler_init(17, 50);
    
    // Assert
    TEST_ASSERT_EQUAL(BUTTON_STATE_IDLE, button_handler_get_state());
    TEST_ASSERT_FALSE(button_handler_is_pressed());
}

void test_button_handler_get_state_should_return_current_state(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    button_state_t state = button_handler_get_state();
    
    // Assert
    TEST_ASSERT_EQUAL(BUTTON_STATE_IDLE, state);
}

void test_button_handler_is_pressed_should_return_false_when_idle(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    bool pressed = button_handler_is_pressed();
    
    // Assert
    TEST_ASSERT_FALSE(pressed);
}

/* ============================================================
 *  Test Group 4: Configuration Tests
 * ============================================================ */

void test_button_handler_should_accept_valid_long_press_threshold(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    int result = button_handler_set_long_press_threshold(3000);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

void test_button_handler_should_reject_too_small_threshold(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    int result = button_handler_set_long_press_threshold(50);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_button_handler_should_fail_set_threshold_when_not_initialized(void) {
    // Act (no init)
    int result = button_handler_set_long_press_threshold(2000);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 5: Process Tests
 * ============================================================ */

void test_button_handler_process_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = button_handler_process();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_button_handler_process_should_succeed_when_initialized(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    int result = button_handler_process();
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

/* ============================================================
 *  Test Group 6: Stop and Cleanup Tests
 * ============================================================ */

void test_button_handler_stop_should_not_crash_when_not_running(void) {
    // Act
    button_handler_stop();
    
    // Assert
    TEST_PASS();
}

void test_button_handler_cleanup_should_not_crash_when_not_initialized(void) {
    // Act
    button_handler_cleanup();
    
    // Assert
    TEST_PASS();
}

void test_button_handler_cleanup_should_reset_state(void) {
    // Arrange
    button_handler_init(17, 50);
    
    // Act
    button_handler_cleanup();
    
    // Assert
    // After cleanup, should be able to init again
    int result = button_handler_init(17, 50);
    TEST_ASSERT_EQUAL(0, result);
}

/* ============================================================
 *  Test Group 7: String Conversion Tests
 * ============================================================ */

void test_button_event_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("NONE", button_event_to_string(BUTTON_EVENT_NONE));
    TEST_ASSERT_EQUAL_STRING("SHORT_PRESS", button_event_to_string(BUTTON_EVENT_SHORT_PRESS));
    TEST_ASSERT_EQUAL_STRING("LONG_PRESS", button_event_to_string(BUTTON_EVENT_LONG_PRESS));
}

void test_button_event_to_string_should_handle_invalid_event(void) {
    // Act
    const char *str = button_event_to_string((button_event_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

void test_button_state_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("IDLE", button_state_to_string(BUTTON_STATE_IDLE));
    TEST_ASSERT_EQUAL_STRING("DEBOUNCING", button_state_to_string(BUTTON_STATE_DEBOUNCING));
    TEST_ASSERT_EQUAL_STRING("PRESSED", button_state_to_string(BUTTON_STATE_PRESSED));
    TEST_ASSERT_EQUAL_STRING("LONG_DETECTED", button_state_to_string(BUTTON_STATE_LONG_DETECTED));
}

void test_button_state_to_string_should_handle_invalid_state(void) {
    // Act
    const char *str = button_state_to_string((button_state_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

/* ============================================================
 *  Test Group 8: Integration Tests (Placeholder)
 * ============================================================ */

// Note: Full integration tests with GPIO mocking will be added
// when we integrate with gaming-core's HAL and GPIO library

void test_button_handler_complete_workflow_placeholder(void) {
    // This is a placeholder for future integration tests
    // that will mock GPIO reads and test full button press workflows
    
    // Arrange
    button_handler_init(17, 50);
    g_callback_count = 0;
    button_handler_set_callback(test_callback, NULL);
    
    // Act
    // TODO: Mock GPIO reads to simulate button press
    // For now, just verify basic operation
    button_handler_process();
    
    // Assert
    TEST_PASS_MESSAGE("Integration test placeholder - needs GPIO mocking");
}
