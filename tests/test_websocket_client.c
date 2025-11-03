/**
 * @file test_websocket_client.c
 * @brief Unit tests for WebSocket Client module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 */

#include "unity.h"
#include "websocket_client.h"
#include <string.h>

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

void setUp(void) {
    // Each test starts with clean state
}

void tearDown(void) {
    // Clean up after each test
    ws_client_cleanup();
}

/* ============================================================
 *  Test Group 1: Initialization Tests
 * ============================================================ */

void test_ws_client_init_should_succeed_with_valid_params(void) {
    // Act
    int result = ws_client_init("192.168.1.1", 8080);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(WS_STATE_DISCONNECTED, ws_client_get_state());
}

void test_ws_client_init_should_fail_with_null_host(void) {
    // Act
    int result = ws_client_init(NULL, 8080);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_ws_client_init_should_fail_with_invalid_port(void) {
    // Act
    int result1 = ws_client_init("192.168.1.1", 0);
    int result2 = ws_client_init("192.168.1.1", 70000);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result1);
    TEST_ASSERT_LESS_THAN(0, result2);
}

void test_ws_client_init_should_fail_when_already_initialized(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    int result = ws_client_init("192.168.1.1", 8080);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 2: Connection Tests
 * ============================================================ */

void test_ws_client_connect_should_succeed(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    int result = ws_client_connect();
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    // In test mode, state immediately becomes CONNECTED
    TEST_ASSERT_EQUAL(WS_STATE_CONNECTED, ws_client_get_state());
}

void test_ws_client_connect_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = ws_client_connect();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_ws_client_connect_should_fail_when_already_connected(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    int result = ws_client_connect();
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 3: Send/Receive Tests
 * ============================================================ */

void test_ws_client_send_should_succeed_when_connected(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    int result = ws_client_send("{\"type\":\"query_ps5\"}");
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

void test_ws_client_send_should_fail_when_not_connected(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act (not connected)
    int result = ws_client_send("{\"type\":\"query_ps5\"}");
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_ws_client_send_should_fail_with_null_message(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    int result = ws_client_send(NULL);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_ws_client_send_should_fail_with_too_large_message(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    char large_message[WS_MAX_MESSAGE_SIZE + 100];
    memset(large_message, 'A', sizeof(large_message) - 1);
    large_message[sizeof(large_message) - 1] = '\0';
    
    // Act
    int result = ws_client_send(large_message);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 4: Callback Tests
 * ============================================================ */

static int g_connected_count = 0;
static int g_disconnected_count = 0;
static int g_error_count = 0;
static int g_message_count = 0;
static char g_last_message[256];

static void test_connected_callback(void *user_data) {
    g_connected_count++;
}

static void test_disconnected_callback(const char *reason, void *user_data) {
    g_disconnected_count++;
}

static void test_error_callback(ws_error_t error, const char *message, void *user_data) {
    g_error_count++;
}

static void test_message_callback(const char *message, size_t length, void *user_data) {
    g_message_count++;
    strncpy(g_last_message, message, sizeof(g_last_message) - 1);
}

void test_ws_client_should_accept_callbacks(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_client_set_callbacks(test_connected_callback,
                           test_disconnected_callback,
                           test_message_callback,
                           test_error_callback,
                           NULL);
    
    // Assert
    TEST_PASS();
}

void test_ws_client_should_trigger_connected_callback(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    g_connected_count = 0;
    ws_client_set_callbacks(test_connected_callback, NULL, NULL, NULL, NULL);
    
    // Act
    ws_client_connect();
    
    // Assert
    TEST_ASSERT_EQUAL(1, g_connected_count);
}

void test_ws_client_should_trigger_disconnected_callback(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    g_disconnected_count = 0;
    ws_client_set_callbacks(NULL, test_disconnected_callback, NULL, NULL, NULL);
    
    // Act
    ws_client_disconnect();
    
    // Assert
    TEST_ASSERT_EQUAL(1, g_disconnected_count);
}

void test_ws_client_should_accept_null_callbacks(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_client_set_callbacks(NULL, NULL, NULL, NULL, NULL);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 5: State Management Tests
 * ============================================================ */

void test_ws_client_get_state_should_return_disconnected_initially(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_state_t state = ws_client_get_state();
    
    // Assert
    TEST_ASSERT_EQUAL(WS_STATE_DISCONNECTED, state);
}

void test_ws_client_get_state_should_return_connected_after_connect(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    ws_state_t state = ws_client_get_state();
    
    // Assert
    TEST_ASSERT_EQUAL(WS_STATE_CONNECTED, state);
}

/* ============================================================
 *  Test Group 6: Reconnection Tests
 * ============================================================ */

void test_ws_client_should_enable_auto_reconnect(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_client_set_auto_reconnect(true);
    
    // Assert
    TEST_PASS();
}

void test_ws_client_should_disable_auto_reconnect(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_client_set_auto_reconnect(false);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 8: Process Tests
 * ============================================================ */

void test_ws_client_process_should_fail_when_not_initialized(void) {
    // Act (no init)
    int result = ws_client_service(100);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_ws_client_process_should_succeed_when_initialized(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    int result = ws_client_service(100);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

void test_ws_client_process_should_handle_connected_state(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    int result = ws_client_service(100);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

/* ============================================================
 *  Test Group 9: Disconnect Tests
 * ============================================================ */

void test_ws_client_disconnect_should_succeed(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    ws_client_disconnect();
    
    // Assert
    TEST_ASSERT_EQUAL(WS_STATE_DISCONNECTED, ws_client_get_state());
}

void test_ws_client_disconnect_should_not_crash_when_not_connected(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    
    // Act
    ws_client_disconnect();
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 10: String Conversion Tests
 * ============================================================ */

void test_ws_state_to_string_should_return_correct_strings(void) {
    // 修正: 使用正確的函數名稱 ws_client_state_to_string
    TEST_ASSERT_EQUAL_STRING("DISCONNECTED", ws_client_state_to_string(WS_STATE_DISCONNECTED));
    TEST_ASSERT_EQUAL_STRING("CONNECTING", ws_client_state_to_string(WS_STATE_CONNECTING));
    TEST_ASSERT_EQUAL_STRING("CONNECTED", ws_client_state_to_string(WS_STATE_CONNECTED));
    TEST_ASSERT_EQUAL_STRING("DISCONNECTING", ws_client_state_to_string(WS_STATE_DISCONNECTING));
    TEST_ASSERT_EQUAL_STRING("ERROR", ws_client_state_to_string(WS_STATE_ERROR));
}

void test_ws_state_to_string_should_handle_invalid_state(void) {
    // Act - 修正: 使用正確的函數名稱
    const char *str = ws_client_state_to_string((ws_state_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

void test_ws_error_to_string_should_return_correct_strings(void) {
    // 修正: 使用正確的函數名稱和錯誤碼
    TEST_ASSERT_EQUAL_STRING("NO_ERROR", ws_client_error_to_string(WS_ERROR_NONE));
    TEST_ASSERT_EQUAL_STRING("CONNECT_FAILED", ws_client_error_to_string(WS_ERROR_CONNECT));
    TEST_ASSERT_EQUAL_STRING("SEND_FAILED", ws_client_error_to_string(WS_ERROR_SEND));
    TEST_ASSERT_EQUAL_STRING("TIMEOUT", ws_client_error_to_string(WS_ERROR_TIMEOUT));
}

void test_ws_error_to_string_should_handle_invalid_error(void) {
    // Act - 修正: 使用正確的函數名稱
    const char *str = ws_client_error_to_string((ws_error_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN_ERROR", str);
}

/* ============================================================
 *  Test Group 11: Cleanup Tests
 * ============================================================ */

void test_ws_client_cleanup_should_not_crash_when_not_initialized(void) {
    // Act
    ws_client_cleanup();
    
    // Assert
    TEST_PASS();
}

void test_ws_client_cleanup_should_reset_state(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();
    
    // Act
    ws_client_cleanup();
    
    // Assert
    // After cleanup, state should be DISCONNECTED
    TEST_ASSERT_EQUAL(WS_STATE_DISCONNECTED, ws_client_get_state());
    
    // Should be able to init again
    int result = ws_client_init("192.168.1.1", 8080);
    TEST_ASSERT_EQUAL(0, result);
}

void test_ws_client_cleanup_should_clear_callbacks(void) {
    // Arrange
    ws_client_init("192.168.1.1", 8080);
    g_connected_count = 0;
    ws_client_set_callbacks(test_connected_callback, NULL, NULL, NULL, NULL);
    
    // Act
    ws_client_cleanup();
    ws_client_init("192.168.1.1", 8080);
    ws_client_connect();  // This should not trigger old callback
    
    // Assert
    TEST_ASSERT_EQUAL(0, g_connected_count);
}
