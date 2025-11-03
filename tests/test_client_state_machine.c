/**
 * @file test_client_state_machine.c
 * @brief Unit tests for Client State Machine module
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 */

#include "unity.h"
#include "client_state_machine.h"
#include <string.h>

/* ============================================================
 *  Test Configuration
 * ============================================================ */

static client_config_t test_config = {
    .button_pin = 17,
    .button_debounce_ms = 50,
    .vpn_socket_path = "/tmp/test_vpn.sock",
    .ws_server_host = "192.168.1.1",
    .ws_server_port = 8080,
    .auto_retry = true,
    .max_retry_attempts = 3,
};

/* ============================================================
 *  Test Fixtures
 * ============================================================ */

static client_context_t *g_ctx = NULL;

void setUp(void) {
    // Create context for each test
    g_ctx = client_sm_create(&test_config);
}

void tearDown(void) {
    // Clean up after each test
    if (g_ctx != NULL) {
        client_sm_destroy(g_ctx);
        g_ctx = NULL;
    }
}

/* ============================================================
 *  Test Group 1: Context Creation Tests
 * ============================================================ */

void test_client_context_create_should_succeed_with_valid_config(void) {
    // Assert (created in setUp)
    TEST_ASSERT_NOT_NULL(g_ctx);
}

void test_client_context_create_should_fail_with_null_config(void) {
    // Act
    client_context_t *ctx = client_sm_create(NULL);
    
    // Assert
    TEST_ASSERT_NULL(ctx);
}

void test_client_context_destroy_should_not_crash_with_null(void) {
    // Act
    client_sm_destroy(NULL);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 2: Initialization Tests
 * ============================================================ */

void test_client_sm_init_should_succeed(void) {
    // Act
    int result = client_sm_init(g_ctx);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_EQUAL(CLIENT_STATE_IDLE, client_sm_get_state(g_ctx));
}

void test_client_sm_init_should_fail_with_null_context(void) {
    // Act
    int result = client_sm_init(NULL);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

void test_client_sm_init_should_fail_when_already_initialized(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    int result = client_sm_init(g_ctx);
    
    // Assert
    TEST_ASSERT_LESS_THAN(0, result);
}

/* ============================================================
 *  Test Group 3: State Query Tests
 * ============================================================ */

void test_client_sm_get_state_should_return_idle_initially(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_state_t state = client_sm_get_state(g_ctx);
    
    // Assert
    TEST_ASSERT_EQUAL(CLIENT_STATE_IDLE, state);
}

void test_client_sm_get_state_should_handle_null_context(void) {
    // Act
    client_state_t state = client_sm_get_state(NULL);
    
    // Assert
    TEST_ASSERT_EQUAL(CLIENT_STATE_IDLE, state);
}

/* ============================================================
 *  Test Group 4: PS5 Status Tests
 * ============================================================ */

void test_client_sm_get_ps5_status_should_return_unknown_initially(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    ps5_status_t status = client_sm_get_ps5_status(g_ctx);
    
    // Assert
    TEST_ASSERT_EQUAL(PS5_STATUS_UNKNOWN, status);
}

void test_client_sm_get_ps5_status_should_handle_null_context(void) {
    // Act
    ps5_status_t status = client_sm_get_ps5_status(NULL);
    
    // Assert
    TEST_ASSERT_EQUAL(PS5_STATUS_UNKNOWN, status);
}

/* ============================================================
 *  Test Group 5: Statistics Tests
 * ============================================================ */

void test_client_sm_get_stats_should_return_zero_initially(void) {
    // Arrange
    client_sm_init(g_ctx);
    client_stats_t stats;
    
    // Act
    client_sm_get_stats(g_ctx, &stats);
    
    // Assert
    TEST_ASSERT_EQUAL(0, stats.button_press_count);
    TEST_ASSERT_EQUAL(0, stats.successful_queries);
    TEST_ASSERT_EQUAL(0, stats.failed_queries);
    TEST_ASSERT_EQUAL(0, stats.error_count);
}

void test_client_sm_get_stats_should_handle_null_context(void) {
    // Arrange
    client_stats_t stats;
    
    // Act
    client_sm_get_stats(NULL, &stats);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_get_stats_should_handle_null_stats_pointer(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_sm_get_stats(g_ctx, NULL);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 6: Callback Tests
 * ============================================================ */

static client_state_t g_callback_old_state;
static client_state_t g_callback_new_state;
static int g_state_callback_count;

static client_error_t g_callback_error;
static int g_error_callback_count;

static void test_state_callback(client_state_t old_state, client_state_t new_state, void *user_data) {
    g_callback_old_state = old_state;
    g_callback_new_state = new_state;
    g_state_callback_count++;
}

static void test_error_callback(client_error_t error, const char *message, void *user_data) {
    g_callback_error = error;
    g_error_callback_count++;
}

void test_client_sm_should_accept_callbacks(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_sm_set_state_callback(g_ctx, test_state_callback, NULL);
    client_sm_set_error_callback(g_ctx, test_error_callback, NULL);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_should_handle_null_callbacks(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_sm_set_state_callback(g_ctx, NULL, NULL);
    client_sm_set_error_callback(g_ctx, NULL, NULL);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_set_callbacks_should_handle_null_context(void) {
    // Act
    client_sm_set_state_callback(NULL, test_state_callback, NULL);
    client_sm_set_error_callback(NULL, test_error_callback, NULL);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 7: Update Tests
 * ============================================================ */

void test_client_sm_update_should_not_crash_when_initialized(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_sm_update(g_ctx);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_update_should_handle_null_context(void) {
    // Act
    client_sm_update(NULL);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_update_should_handle_uninitialized_context(void) {
    // Act (not initialized)
    client_sm_update(g_ctx);
    
    // Assert
    TEST_PASS();
}

/* ============================================================
 *  Test Group 8: Cleanup Tests
 * ============================================================ */

void test_client_sm_cleanup_should_not_crash_with_null_context(void) {
    // Act
    client_sm_cleanup(NULL);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_cleanup_should_not_crash_when_not_initialized(void) {
    // Act
    client_sm_cleanup(g_ctx);
    
    // Assert
    TEST_PASS();
}

void test_client_sm_cleanup_should_allow_reinit(void) {
    // Arrange
    client_sm_init(g_ctx);
    
    // Act
    client_sm_cleanup(g_ctx);
    int result = client_sm_init(g_ctx);
    
    // Assert
    TEST_ASSERT_EQUAL(0, result);
}

/* ============================================================
 *  Test Group 9: String Conversion Tests
 * ============================================================ */

void test_client_state_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("IDLE", client_state_to_string(CLIENT_STATE_IDLE));
    TEST_ASSERT_EQUAL_STRING("VPN_CONNECTING", client_state_to_string(CLIENT_STATE_VPN_CONNECTING));
    TEST_ASSERT_EQUAL_STRING("VPN_CONNECTED", client_state_to_string(CLIENT_STATE_VPN_CONNECTED));
    TEST_ASSERT_EQUAL_STRING("WS_CONNECTING", client_state_to_string(CLIENT_STATE_WS_CONNECTING));
    TEST_ASSERT_EQUAL_STRING("QUERYING_PS5", client_state_to_string(CLIENT_STATE_QUERYING_PS5));
    TEST_ASSERT_EQUAL_STRING("LED_UPDATE", client_state_to_string(CLIENT_STATE_LED_UPDATE));
    TEST_ASSERT_EQUAL_STRING("WAITING", client_state_to_string(CLIENT_STATE_WAITING));
    TEST_ASSERT_EQUAL_STRING("ERROR", client_state_to_string(CLIENT_STATE_ERROR));
    TEST_ASSERT_EQUAL_STRING("CLEANUP", client_state_to_string(CLIENT_STATE_CLEANUP));
}

void test_client_state_to_string_should_handle_invalid_state(void) {
    // Act
    const char *str = client_state_to_string((client_state_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", str);
}

void test_client_error_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("NO_ERROR", client_error_to_string(CLIENT_ERROR_NONE));
    TEST_ASSERT_EQUAL_STRING("VPN_TIMEOUT", client_error_to_string(CLIENT_ERROR_VPN_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("VPN_FAILED", client_error_to_string(CLIENT_ERROR_VPN_FAILED));
    TEST_ASSERT_EQUAL_STRING("WS_TIMEOUT", client_error_to_string(CLIENT_ERROR_WS_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("WS_FAILED", client_error_to_string(CLIENT_ERROR_WS_FAILED));
    TEST_ASSERT_EQUAL_STRING("PS5_TIMEOUT", client_error_to_string(CLIENT_ERROR_PS5_TIMEOUT));
    TEST_ASSERT_EQUAL_STRING("PS5_FAILED", client_error_to_string(CLIENT_ERROR_PS5_FAILED));
    TEST_ASSERT_EQUAL_STRING("MAX_RETRIES", client_error_to_string(CLIENT_ERROR_MAX_RETRIES));
}

void test_client_error_to_string_should_handle_invalid_error(void) {
    // Act
    const char *str = client_error_to_string((client_error_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("UNKNOWN_ERROR", str);
}

void test_client_ps5_status_to_string_should_return_correct_strings(void) {
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", ps5_status_to_string(PS5_STATUS_UNKNOWN));
    TEST_ASSERT_EQUAL_STRING("OFF", ps5_status_to_string(PS5_STATUS_OFF));
    TEST_ASSERT_EQUAL_STRING("STANDBY", ps5_status_to_string(PS5_STATUS_STANDBY));
    TEST_ASSERT_EQUAL_STRING("ON", ps5_status_to_string(PS5_STATUS_ON));
}

void test_client_ps5_status_to_string_should_handle_invalid_status(void) {
    // Act
    const char *str = ps5_status_to_string((ps5_status_t)999);
    
    // Assert
    TEST_ASSERT_EQUAL_STRING("INVALID", str);
}

/* ============================================================
 *  Test Group 10: Integration Tests (Simple)
 * ============================================================ */

void test_client_sm_complete_workflow_placeholder(void) {
    // This is a placeholder for integration tests
    // Full integration tests would require mocking all submodules
    
    // Arrange
    client_sm_init(g_ctx);
    g_state_callback_count = 0;
    client_sm_set_state_callback(g_ctx, test_state_callback, NULL);
    client_sm_set_error_callback(g_ctx, test_error_callback, NULL);
    
    // Act
    client_sm_update(g_ctx);
    
    // Assert
    TEST_ASSERT_EQUAL(CLIENT_STATE_IDLE, client_sm_get_state(g_ctx));
    TEST_PASS_MESSAGE("Integration test placeholder - needs full mocking");
}
