/**
 * @file main.c
 * @brief Gaming Client Main Daemon (最終完全修正版 v3)
 * 
 * 修正內容:
 * 1. log_xxx() -> logger_xxx()
 * 2. logger_init() 添加第三個參數
 * 3. client_sm_context_t -> client_context_t
 * 4. 使用 client_sm_create() 創建 context (不直接 calloc)
 * 5. 移除重複的 client_config_t 定義
 * 6. 不直接存取 context 內部成員
 * 
 * @version 1.0.3
 * @date 2025-11-04
 */

#include "client_state_machine.h"
#include "button_handler.h"
#include "vpn_controller.h"
#include "websocket_client.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/logger.h>
    #include <gaming/led_controller.h>
    #include <gaming/config_parser.h>
    #include <gaming/hal_interface.h>
  #else
    #include "../../gaming-core/src/logger.h"
    #include "../../gaming-core/src/led_controller.h"
    #include "../../gaming-core/src/config_parser.h"
    #include "../../gaming-core/src/hal_interface.h"
  #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

/* ============================================================
 *  Constants
 * ============================================================ */

#define PROGRAM_NAME    "gaming-client"
#define PROGRAM_VERSION "1.0.3"

// Default configuration values
#define DEFAULT_BUTTON_PIN          17
#define DEFAULT_BUTTON_DEBOUNCE_MS  50
#define DEFAULT_LED_PIN_R           22
#define DEFAULT_LED_PIN_G           23
#define DEFAULT_LED_PIN_B           24
#define DEFAULT_VPN_SOCKET_PATH     "/var/run/vpn-agent.sock"
#define DEFAULT_WS_SERVER_HOST      "192.168.1.1"
#define DEFAULT_WS_SERVER_PORT      8080

/* ============================================================
 *  Global Variables
 * ============================================================ */

static volatile sig_atomic_t g_running = 1;
static client_context_t *g_client_ctx = NULL;

/* ============================================================
 *  LED Configuration Structure
 *  (分離出來避免與 client_config_t 混淆)
 * ============================================================ */

typedef struct {
    int led_pin_r;
    int led_pin_g;
    int led_pin_b;
} led_config_local_t;

/* ============================================================
 *  Signal Handling
 * ============================================================ */

static void signal_handler(int signum) {
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            logger_info("Received signal %d, shutting down gracefully...", signum);
            g_running = 0;
            break;
            
        case SIGUSR1:
            // Simulate button press for testing
            logger_info("Received SIGUSR1, simulating button press");
            if (g_client_ctx) {
                // 使用正確的 API: client_sm_trigger_button
                client_sm_trigger_button(g_client_ctx, false);
            }
            break;
            
        default:
            break;
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    
    // Ignore SIGPIPE
    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 *  Callback Functions
 * ============================================================ */

static void on_state_change(client_state_t old_state, 
                           client_state_t new_state, 
                           void *user_data) {
    logger_info("Client state: %s -> %s",
                client_state_to_string(old_state),
                client_state_to_string(new_state));
}

static void on_error(client_error_t error, const char *message, void *user_data) {
    logger_error("Client error: %s - %s", client_error_to_string(error), message);
}

/* ============================================================
 *  Configuration Loading
 * ============================================================ */

static int load_configuration(client_config_t *config, led_config_local_t *led_config) {
    if (config == NULL || led_config == NULL) {
        return -1;
    }
    
    // Initialize with defaults
    config->button_pin = DEFAULT_BUTTON_PIN;
    config->button_debounce_ms = DEFAULT_BUTTON_DEBOUNCE_MS;
    strncpy(config->vpn_socket_path, DEFAULT_VPN_SOCKET_PATH, sizeof(config->vpn_socket_path) - 1);
    strncpy(config->ws_server_host, DEFAULT_WS_SERVER_HOST, sizeof(config->ws_server_host) - 1);
    config->ws_server_port = DEFAULT_WS_SERVER_PORT;
    config->auto_retry = true;
    config->max_retry_attempts = 3;
    
    led_config->led_pin_r = DEFAULT_LED_PIN_R;
    led_config->led_pin_g = DEFAULT_LED_PIN_G;
    led_config->led_pin_b = DEFAULT_LED_PIN_B;
    
    // Initialize config parser
    if (config_parser_init() != 0) {
        logger_warning("Failed to initialize config parser, using defaults");
        return 0;  // Not fatal, use defaults
    }
    
    // Try to load from UCI
    int value;
    char str_value[256];
    
    // Button configuration
    if (config_parser_get_int("gaming-client", "hardware", "button_pin", &value) == 0) {
        config->button_pin = value;
    }
    
    if (config_parser_get_int("gaming-client", "hardware", "button_debounce_ms", &value) == 0) {
        config->button_debounce_ms = value;
    }
    
    // LED configuration
    if (config_parser_get_int("gaming-client", "hardware", "led_pin_r", &value) == 0) {
        led_config->led_pin_r = value;
    }
    if (config_parser_get_int("gaming-client", "hardware", "led_pin_g", &value) == 0) {
        led_config->led_pin_g = value;
    }
    if (config_parser_get_int("gaming-client", "hardware", "led_pin_b", &value) == 0) {
        led_config->led_pin_b = value;
    }
    
    // VPN configuration
    if (config_parser_get_string("gaming-client", "network", "vpn_socket_path",
                                 str_value, sizeof(str_value)) == 0) {
        strncpy(config->vpn_socket_path, str_value, sizeof(config->vpn_socket_path) - 1);
    }
    
    // WebSocket configuration
    if (config_parser_get_string("gaming-client", "network", "ws_server_host",
                                 str_value, sizeof(str_value)) == 0) {
        strncpy(config->ws_server_host, str_value, sizeof(config->ws_server_host) - 1);
    }
    
    if (config_parser_get_int("gaming-client", "network", "ws_server_port", &value) == 0) {
        config->ws_server_port = value;
    }
    
    // Retry configuration
    bool bool_value;
    if (config_parser_get_bool("gaming-client", "network", "auto_retry", &bool_value) == 0) {
        config->auto_retry = bool_value;
    }
    
    if (config_parser_get_int("gaming-client", "network", "max_retry_attempts", &value) == 0) {
        config->max_retry_attempts = value;
    }
    
    return 0;
}

/* ============================================================
 *  System Initialization
 * ============================================================ */

static int initialize_system(const client_config_t *config, 
                             const led_config_local_t *led_config,
                             bool use_mock) {
    int result;
    
    // 1. Initialize logger
    logger_init(PROGRAM_NAME, LOG_LEVEL_INFO, LOG_TARGET_SYSLOG);
    logger_info("=== Gaming Client Starting ===");
    logger_info("Version: %s", PROGRAM_VERSION);
    logger_info("Mode: %s", use_mock ? "MOCK" : "REAL");
    
    // 2. Initialize HAL
    #ifndef TESTING
    result = hal_init(use_mock ? "mock" : "real");
    if (result != 0) {
        logger_error("Failed to initialize HAL");
        return -1;
    }
    logger_info("HAL initialized successfully");
    #endif
    
    // 3. Initialize LED controller
    #ifndef TESTING
    led_config_t led_cfg = {
        .pin_r = led_config->led_pin_r,
        .pin_g = led_config->led_pin_g,
        .pin_b = led_config->led_pin_b
    };
    
    result = led_controller_init(&led_cfg);
    if (result != 0) {
        logger_error("Failed to initialize LED controller");
        hal_cleanup();
        return -1;
    }
    logger_info("LED controller initialized (R:%d, G:%d, B:%d)",
                led_config->led_pin_r, led_config->led_pin_g, led_config->led_pin_b);
    #endif
    
    // 4. Create client context using API (修正: 使用 client_sm_create)
    g_client_ctx = client_sm_create(config);
    if (g_client_ctx == NULL) {
        logger_error("Failed to create client context");
        #ifndef TESTING
        led_controller_deinit();
        hal_cleanup();
        #endif
        return -1;
    }
    logger_info("Client context created");
    
    // 5. Initialize state machine
    result = client_sm_init(g_client_ctx);
    if (result != 0) {
        logger_error("Failed to initialize state machine");
        client_sm_destroy(g_client_ctx);
        g_client_ctx = NULL;
        #ifndef TESTING
        led_controller_deinit();
        hal_cleanup();
        #endif
        return -1;
    }
    logger_info("State machine initialized");
    
    // 6. Set callbacks
    client_sm_set_state_callback(g_client_ctx, on_state_change, NULL);
    client_sm_set_error_callback(g_client_ctx, on_error, NULL);
    
    // 7. Initialize button handler
    result = button_handler_init(config->button_pin, config->button_debounce_ms);
    if (result != 0) {
        logger_error("Failed to initialize button handler");
        client_sm_destroy(g_client_ctx);
        g_client_ctx = NULL;
        #ifndef TESTING
        led_controller_deinit();
        hal_cleanup();
        #endif
        return -1;
    }
    logger_info("Button handler initialized (pin:%d, debounce:%dms)",
                config->button_pin, config->button_debounce_ms);
    
    // 8. Initialize VPN controller
    result = vpn_controller_init(config->vpn_socket_path);
    if (result != 0) {
        logger_warning("Failed to initialize VPN controller (will retry later)");
        // Not fatal - VPN might not be running yet
    } else {
        logger_info("VPN controller initialized (socket:%s)", config->vpn_socket_path);
    }
    
    // 9. Initialize WebSocket client
    result = ws_client_init(config->ws_server_host, config->ws_server_port);
    if (result != 0) {
        logger_warning("Failed to initialize WebSocket client (will retry later)");
        // Not fatal - server might not be running yet
    } else {
        logger_info("WebSocket client initialized (server:%s:%d)",
                    config->ws_server_host, config->ws_server_port);
    }
    
    logger_info("=== System initialization complete ===");
    return 0;
}

/* ============================================================
 *  System Cleanup
 * ============================================================ */

static void cleanup_system(void) {
    logger_info("=== Gaming Client Shutting Down ===");
    
    // Cleanup in reverse order
    ws_client_cleanup();
    logger_info("WebSocket client cleaned up");
    
    vpn_controller_cleanup();
    logger_info("VPN controller cleaned up");
    
    button_handler_cleanup();
    logger_info("Button handler cleaned up");
    
    if (g_client_ctx) {
        client_sm_destroy(g_client_ctx);
        g_client_ctx = NULL;
        logger_info("State machine cleaned up");
    }
    
    #ifndef TESTING
    led_controller_deinit();
    logger_info("LED controller cleaned up");
    
    hal_cleanup();
    logger_info("HAL cleaned up");
    #endif
    
    config_parser_cleanup();
    
    logger_info("=== Shutdown complete ===");
    logger_cleanup();
}

/* ============================================================
 *  Main Event Loop
 * ============================================================ */

static void run_main_loop(void) {
    logger_info("Entering main event loop");
    
    while (g_running) {
        // Update state machine
        if (g_client_ctx) {
            client_sm_update(g_client_ctx);
        }
        
        // Process button events
        button_handler_process();
        
        // Service WebSocket
        ws_client_service(10);  // 10ms timeout
        
        // Small delay to prevent CPU hogging
        usleep(10000);  // 10ms
    }
    
    logger_info("Exiting main event loop");
}

/* ============================================================
 *  Main Entry Point
 * ============================================================ */

static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --daemon        Run as daemon\n");
    printf("  -m, --mock          Use mock hardware (for testing)\n");
    printf("  -v, --version       Print version and exit\n");
    printf("  -h, --help          Print this help and exit\n");
    printf("\nExamples:\n");
    printf("  %s                  # Run in foreground\n", program_name);
    printf("  %s --daemon         # Run as daemon\n", program_name);
    printf("  %s --mock           # Run with mock hardware\n", program_name);
}

static void print_version(void) {
    printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    bool use_mock = false;
    client_config_t config;
    led_config_local_t led_config;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"daemon",  no_argument, 0, 'd'},
        {"mock",    no_argument, 0, 'm'},
        {"version", no_argument, 0, 'v'},
        {"help",    no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "dmvh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = true;
                break;
            case 'm':
                use_mock = true;
                break;
            case 'v':
                print_version();
                return 0;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Daemonize if requested
    if (daemon_mode) {
        if (daemon(0, 0) != 0) {
            perror("daemon");
            return 1;
        }
    }
    
    // Setup signal handlers
    setup_signal_handlers();
    
    // Load configuration
    if (load_configuration(&config, &led_config) != 0) {
        fprintf(stderr, "Failed to load configuration\n");
        return 1;
    }
    
    // Initialize system
    if (initialize_system(&config, &led_config, use_mock) != 0) {
        fprintf(stderr, "Failed to initialize system\n");
        return 1;
    }
    
    // Run main loop
    run_main_loop();
    
    // Cleanup
    cleanup_system();
    
    return 0;
}
