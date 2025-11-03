/**
 * @file main.c
 * @brief Gaming Client Daemon - Main Entry Point
 * 
 * This is the main daemon for the gaming client. It integrates all modules:
 * - Button handler for user input
 * - VPN controller for VPN connection
 * - WebSocket client for server communication
 * - Client state machine for orchestration
 * - LED controller for status indication
 * 
 * @author Gaming System Development Team
 * @date 2025-11-03
 * @version 1.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>

#include "client_state_machine.h"

#ifndef TESTING
  #ifdef OPENWRT_BUILD
    #include <gaming/hal_interface.h>
    #include <gaming/logger.h>
    #include <gaming/config_parser.h>
  #else
    #include "../../gaming-core/src/hal_interface.h"
    #include "../../gaming-core/src/logger.h"
    #include "../../gaming-core/src/config_parser.h"
  #endif
#endif

/* ============================================================
 *  Constants and Macros
 * ============================================================ */

#define PROGRAM_NAME        "gaming-client"
#define PROGRAM_VERSION     "1.0.0"
#define MAIN_LOOP_DELAY_MS  10

/* ============================================================
 *  Global Variables
 * ============================================================ */

static volatile bool g_running = true;
static client_context_t *g_client_ctx = NULL;

/* ============================================================
 *  Signal Handlers
 * ============================================================ */

/**
 * @brief Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    switch (signum) {
        case SIGTERM:
        case SIGINT:
            #ifndef TESTING
            log_info("Received signal %d, shutting down gracefully...", signum);
            #else
            printf("Received signal %d\n", signum);
            #endif
            g_running = false;
            break;
            
        case SIGUSR1:
            // For testing: simulate button press
            #ifndef TESTING
            log_info("Received SIGUSR1, simulating button press");
            #else
            printf("Simulating button press\n");
            #endif
            break;
            
        default:
            break;
    }
}

/**
 * @brief Setup signal handlers
 */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    
    // Ignore SIGPIPE (common for socket operations)
    signal(SIGPIPE, SIG_IGN);
}

/* ============================================================
 *  Callback Functions
 * ============================================================ */

/**
 * @brief State change callback
 */
static void on_state_change(client_state_t old_state, client_state_t new_state, void *user_data) {
    #ifndef TESTING
    log_info("State transition: %s -> %s",
             client_state_to_string(old_state),
             client_state_to_string(new_state));
    #else
    printf("State: %s -> %s\n", 
           client_state_to_string(old_state),
           client_state_to_string(new_state));
    #endif
}

/**
 * @brief Error callback
 */
static void on_error(client_error_t error, const char *message, void *user_data) {
    #ifndef TESTING
    log_error("Client error: %s - %s", client_error_to_string(error), message);
    #else
    fprintf(stderr, "Error: %s - %s\n", client_error_to_string(error), message);
    #endif
}

/* ============================================================
 *  Initialization and Cleanup
 * ============================================================ */

/**
 * @brief Load configuration from UCI or use defaults
 */
static int load_configuration(client_config_t *config) {
    #ifndef TESTING
    gaming_config_t core_config;
    
    if (config_parser_load(&core_config) < 0) {
        log_warn("Failed to load configuration, using defaults");
        // Use defaults
    }
    
    // Map core config to client config
    config->button_pin = core_config.button_pin;
    config->button_debounce_ms = core_config.debounce_ms;
    
    // VPN socket path
    strncpy(config->vpn_socket_path, PATH_VPN_SOCKET, sizeof(config->vpn_socket_path) - 1);
    
    // WebSocket server (should be read from config)
    strncpy(config->ws_server_host, "192.168.1.1", sizeof(config->ws_server_host) - 1);
    config->ws_server_port = WEBSOCKET_PORT;
    
    config->auto_retry = true;
    config->max_retry_attempts = 3;
    
    log_info("Configuration loaded: button_pin=%d, ws_server=%s:%d",
             config->button_pin, config->ws_server_host, config->ws_server_port);
    #else
    // Test mode defaults
    config->button_pin = 17;
    config->button_debounce_ms = 50;
    strncpy(config->vpn_socket_path, "/tmp/vpn.sock", sizeof(config->vpn_socket_path) - 1);
    strncpy(config->ws_server_host, "127.0.0.1", sizeof(config->ws_server_host) - 1);
    config->ws_server_port = 8080;
    config->auto_retry = true;
    config->max_retry_attempts = 3;
    #endif
    
    return 0;
}

/**
 * @brief Initialize all modules
 */
static int initialize_system(bool use_mock_hal) {
    #ifndef TESTING
    // Initialize HAL
    const char *hal_mode = use_mock_hal ? "mock" : "real";
    if (hal_init(hal_mode) < 0) {
        fprintf(stderr, "Failed to initialize HAL: %s\n", hal_mode);
        return -1;
    }
    log_info("HAL initialized: %s mode", hal_mode);
    
    // Initialize logger
    logger_init(PROGRAM_NAME, LOG_LEVEL_INFO);
    log_info("Gaming client daemon starting (version %s)", PROGRAM_VERSION);
    #else
    printf("Gaming client daemon starting (test mode)\n");
    #endif
    
    // Load configuration
    client_config_t config;
    if (load_configuration(&config) < 0) {
        #ifndef TESTING
        log_error("Failed to load configuration");
        #endif
        return -1;
    }
    
    // Create client context
    g_client_ctx = client_sm_create(&config);
    if (g_client_ctx == NULL) {
        #ifndef TESTING
        log_error("Failed to create client context");
        #endif
        return -1;
    }
    
    // Initialize state machine
    if (client_sm_init(g_client_ctx) < 0) {
        #ifndef TESTING
        log_error("Failed to initialize state machine");
        #endif
        client_context_destroy(g_client_ctx);
        g_client_ctx = NULL;
        return -1;
    }
    
    // Set callbacks
    client_sm_set_state_callback(g_client_ctx, on_state_change, NULL);
    client_sm_set_error_callback(g_client_ctx, on_error, NULL);
    
    #ifndef TESTING
    log_info("All modules initialized successfully");
    #else
    printf("All modules initialized\n");
    #endif
    
    return 0;
}

/**
 * @brief Cleanup all modules
 */
static void cleanup_system(void) {
    #ifndef TESTING
    log_info("Cleaning up...");
    #else
    printf("Cleaning up...\n");
    #endif
    
    // Cleanup client context
    if (g_client_ctx != NULL) {
        client_sm_destroy(g_client_ctx);
        g_client_ctx = NULL;
    }
    
    #ifndef TESTING
    // Cleanup HAL
    hal_cleanup();
    
    // Cleanup logger
    logger_cleanup();
    #endif
    
    #ifndef TESTING
    log_info("Gaming client daemon stopped");
    #else
    printf("Gaming client daemon stopped\n");
    #endif
}

/* ============================================================
 *  Main Loop
 * ============================================================ */

/**
 * @brief Main event loop
 */
static void main_loop(void) {
    #ifndef TESTING
    log_info("Entering main loop");
    #else
    printf("Entering main loop\n");
    #endif
    
    while (g_running) {
        // Update state machine (this processes all submodules)
        client_sm_update(g_client_ctx);
        
        // Small delay to prevent CPU spinning
        usleep(MAIN_LOOP_DELAY_MS * 1000);
    }
    
    #ifndef TESTING
    log_info("Exiting main loop");
    #else
    printf("Exiting main loop\n");
    #endif
}

/**
 * @brief Display usage information
 */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\n");
    printf("Gaming Client Daemon - Controls VPN and queries PS5 status\n");
    printf("\n");
    printf("Options:\n");
    printf("  -m, --mock         Use mock HAL (for testing)\n");
    printf("  -d, --daemon       Run as daemon (background)\n");
    printf("  -h, --help         Display this help and exit\n");
    printf("  -v, --version      Display version information and exit\n");
    printf("\n");
    printf("Signals:\n");
    printf("  SIGTERM, SIGINT    Graceful shutdown\n");
    printf("  SIGUSR1            Simulate button press (testing)\n");
    printf("\n");
}

/**
 * @brief Display version information
 */
static void print_version(void) {
    printf("%s version %s\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Gaming System Client Daemon\n");
    printf("Copyright (C) 2025 Gaming System Development Team\n");
}

/**
 * @brief Daemonize the process
 */
static int daemonize(void) {
    pid_t pid = fork();
    
    if (pid < 0) {
        return -1;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(EXIT_SUCCESS);
    }
    
    // Child continues
    
    // Create new session
    if (setsid() < 0) {
        return -1;
    }
    
    // Change working directory
    if (chdir("/") < 0) {
        return -1;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    return 0;
}

/* ============================================================
 *  Main Entry Point
 * ============================================================ */

int main(int argc, char *argv[]) {
    bool use_mock_hal = false;
    bool run_as_daemon = false;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"mock",    no_argument, 0, 'm'},
        {"daemon",  no_argument, 0, 'd'},
        {"help",    no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "mdhv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'm':
                use_mock_hal = true;
                break;
                
            case 'd':
                run_as_daemon = true;
                break;
                
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
                
            case 'v':
                print_version();
                return EXIT_SUCCESS;
                
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }
    
    // Daemonize if requested
    if (run_as_daemon) {
        if (daemonize() < 0) {
            fprintf(stderr, "Failed to daemonize\n");
            return EXIT_FAILURE;
        }
    }
    
    // Setup signal handlers
    setup_signal_handlers();
    
    // Initialize system
    if (initialize_system(use_mock_hal) < 0) {
        fprintf(stderr, "Initialization failed\n");
        return EXIT_FAILURE;
    }
    
    // Run main loop
    main_loop();
    
    // Cleanup
    cleanup_system();
    
    return EXIT_SUCCESS;
}
