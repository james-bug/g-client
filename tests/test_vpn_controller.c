/**
 * @file test_vpn_controller.c
 * @brief Unit tests for VPN Controller module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 */

#include "unity.h"
#include "vpn_controller.h"
#include <string.h>

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

void setUp(void) {
    // Each test starts with clean state
}

void tearDown(void) {
    // Clean up after each test
    vpn_controller_cleanup();
}

/* ============================================================
 *  Test Group 1: Initialization Tests
 * ============================================================ */

void test_vpn_controller_init_should_succeed_with_default_path(void) {
    // Act
    int result = vpn_controller_init(NULL);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(VPN_STATE_DISCONNECTED, vpn_controller_get_state());
}

void test_vpn_controller_init_should_succeed_with_custom_path(void) {
    // Act
    int result = vpn_controller_init("/tmp/test_vpn.sock");
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(VPN_STATE_DISCONNECTED, vpn_controller_get_state());
}

void test_vpn_controller_init_should_fail_when_already_initialized(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    int result = vpn_controller_init(NULL);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 2: Connection Tests
 * ============================================================ */

void test_vpn_controller_connect_should_succeed(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    int result = vpn_controller_connect();
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(VPN_STATE_CONNECTING, vpn_controller_get_state());
}

void test_vpn_controller_connect_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = vpn_controller_connect();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_vpn_controller_connect_should_fail_when_already_connecting(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_controller_connect();
    
    // Act
    int result = vpn_controller_connect();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 3: Disconnection Tests
 * ============================================================ */

void test_vpn_controller_disconnect_should_succeed(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_controller_connect();
    vpn_controller_process(100);  // Simulate connection established
    
    // Act
    int result = vpn_controller_disconnect();
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(VPN_STATE_DISCONNECTING, vpn_controller_get_state());
}

void test_vpn_controller_disconnect_should_succeed_when_already_disconnected(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    int result = vpn_controller_disconnect();
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

void test_vpn_controller_disconnect_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = vpn_controller_disconnect();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 4: State Query Tests
 * ============================================================ */

void test_vpn_controller_get_state_should_return_initial_state(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    vpn_state_t state = vpn_controller_get_state();
    
    // Assert
    TEST_ASSERT_EQUAL(VPN_STATE_DISCONNECTED, state);
}

void test_vpn_controller_get_state_should_return_connecting_state(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_controller_connect();
    
    // Act
    vpn_state_t state = vpn_controller_get_state();
    
    // Assert
    TEST_ASSERT_EQUAL(VPN_STATE_CONNECTING, state);
}

/* ============================================================
 *  Test Group 5: Info Query Tests
 * ============================================================ */

void test_vpn_controller_get_info_should_fail_when_not_initialized(void) {
    // Arrange
    vpn_info_t info;
    
    // Act (no init)
    int result = vpn_controller_get_info(&info);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_vpn_controller_get_info_should_fail_with_null_pointer(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    int result = vpn_controller_get_info(NULL);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_vpn_controller_get_info_should_succeed(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_info_t info;
    
    // Act
    int result = vpn_controller_get_info(&info);
    
    // Assert
    // In test mode, this should succeed with mock data
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(VPN_STATE_CONNECTED, info.state);
}

/* ============================================================
 *  Test Group 6: Callback Tests
 * ============================================================ */

static vpn_state_t g_callback_old_state;
static vpn_state_t g_callback_new_state;
static int g_callback_count;

static void test_callback(vpn_state_t old_state, vpn_state_t new_state, void *user_data) {
    g_callback_old_state = old_state;
    g_callback_new_state = new_state;
    g_callback_count++;
}

void test_vpn_controller_should_accept_callback(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    vpn_controller_set_callback(test_callback, NULL);
    
    // Assert
    TEST_PASS();
}

void test_vpn_controller_should_trigger_callback_on_state_change(void) {
    // Arrange
    vpn_controller_init(NULL);
    g_callback_count = 0;
    vpn_controller_set_callback(test_callback, NULL);
    
    // Act
    vpn_controller_connect();
    
    // Assert
    TEST_ASSERT_EQUAL(1, g_callback_count);
    TEST_ASSERT_EQUAL(VPN_STATE_DISCONNECTED, g_callback_old_state);
    TEST_ASSERT_EQUAL(VPN_STATE_CONNECTING, g_callback_new_state);
}

void test_vpn_controller_should_accept_null_callback(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    vpn_controller_set_callback(NULL, NULL);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 7: Process Tests
 * ============================================================ */

void test_vpn_controller_process_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = vpn_controller_process(100);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_vpn_controller_process_should_succeed_when_no_pending_operation(void) {
    // Arrange
    vpn_controller_init(NULL);
    
    // Act
    int result = vpn_controller_process(100);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

void test_vpn_controller_process_should_handle_pending_operation(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_controller_connect();
    
    // Act
    int result = vpn_controller_process(100);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    // In test mode with mock response, state should change to CONNECTED
    TEST_ASSERT_EQUAL(VPN_STATE_CONNECTED, vpn_controller_get_state());
}

/* ============================================================
 *  Test Group 8: String Conversion Tests
 * ============================================================ */

void test_vpn_state_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", vpn_controller_state_to_string(VPN_STATE_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("DISCONNECTED", vpn_controller_state_to_string(VPN_STATE_DISCONNECTED));
    TEST_ASSERT_EQUAL_STRING("CONNECTING", vpn_controller_state_to_string(VPN_STATE_CONNECTING));
    TEST_ASSERT_EQUAL_STRING("CONNECTED", vpn_controller_state_to_string(VPN_STATE_CONNECTED));
    TEST_ASSERT_EQUAL_STRING("DISCONNECTING", vpn_controller_state_to_string(VPN_STATE_DISCONNECTING));
    TEST_ASSERT_EQUAL_STRING("ERROR", vpn_controller_state_to_string(VPN_STATE_ERROR));
}

void test_vpn_state_to_string_should_handle_invalid_state(void) {
    // Act
    const char *str = vpn_controller_state_to_string((vpn_state_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("INVALID", str);
}

void test_vpn_error_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("NO_ERROR", vpn_controller_error_to_string(VPN_ERROR_NONE));
    TEST_ASSERT_EQUAL_STRING("SOCKET_ERROR", vpn_controller_error_to_string(VPN_ERROR_SOCKET));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", vpn_controller_error_to_string(VPN_ERROR_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("AGENT_UNREACHABLE", vpn_controller_error_to_string(VPN_ERROR_AGENT_UNREACHABLE));
    TEST_ASSERT_EQUAL_STRING("INVALID_RESPONSE", vpn_controller_error_to_string(VPN_ERROR_INVALID_RESPONSE));
}

void test_vpn_error_to_string_should_handle_invalid_error(void) {
    // Act
    const char *str = vpn_controller_error_to_string((vpn_error_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN_ERROR", str);
}

/* ============================================================
 *  Test Group 9: Cleanup Tests
 * ============================================================ */

void test_vpn_controller_cleanup_should_not_crash_when_not_initialized(void) {
    // Act
    vpn_controller_cleanup();
    
    // Assert
    TEST_PASS();
}

void test_vpn_controller_cleanup_should_reset_state(void) {
    // Arrange
    vpn_controller_init(NULL);
    vpn_controller_connect();
    
    // Act
    vpn_controller_cleanup();
    
    // Assert
    // After cleanup, state should be UNKNOWN
    TEST_ASSERT_EQUAL(VPN_STATE_UNKNOWN, vpn_controller_get_state());
    
    // Should be able to init again
    int result = vpn_controller_init(NULL);
    TEST_ASSERT_EQUAL(0, result);
}

void test_vpn_controller_cleanup_should_clear_callback(void) {
    // Arrange
    vpn_controller_init(NULL);
    g_callback_count = 0;
    vpn_controller_set_callback(test_callback, NULL);
    
    // Act
    vpn_controller_cleanup();
    vpn_controller_init(NULL);
    vpn_controller_connect();  // This should not trigger old callback
    
    // Assert
    TEST_ASSERT_EQUAL(0, g_callback_count);
}
