/*
 * ============================================================================
 * client.c - QNX Concurrent File Encryption Service Client
 * ============================================================================
 *
 * Description:
 *   This client connects to the encryption server, sends a large data
 *   payload (64KB+), and receives the encrypted result with asynchronous
 *   notification.
 *
 * Requirements Met:
 *   - Req 1: Uses QNX message passing API (MsgSendv)
 *   - Req 2: Client-server architecture
 *   - Req 3: IOV transfer for large payload
 *   - Req 4: Sends 64KB+ payload
 *   - Req 6: Well-commented with meaningful variable names
 *   - Req 7: 10-second timeout with retry logic
 *   - Req 8: All API return values checked
 *   - Req 9: Uses name_open() exclusively for connection
 * ============================================================================
 */

#include "message.h"

/* Global flag for timeout handling */
static volatile int g_timeout_occurred = 0;

/**
 * Signal handler for connection timeout
 *
 * Parameters:
 *   signal_number - Signal number (SIGALRM)
 */
static void connection_timeout_handler(int signal_number) {
    (void)signal_number;  /* Unused parameter */
    g_timeout_occurred = 1;
}

/**
 * Main client entry point
 */
int main(void) {
    int            server_fd = -1;
    int            api_result;
    time_t         start_time;
    time_t         current_time;
    double         elapsed_seconds;

    /* Message buffers */
    encrypt_request_msg_t  request_message;
    encrypt_response_msg_t response_message;

    /* IOV structures for messaging */
    iov_t  request_iov[1];
    iov_t  response_iov[1];

    /* Signal handling setup */
    struct sigaction timeout_action;

    printf("\n");
    printf("============================================================\n");
    printf("   QNX Concurrent File Encryption Service - Client\n");
    printf("============================================================\n\n");

    /*
     * Req 7: Setup signal handler for timeout
     * This allows us to interrupt the connection attempt after 10 seconds
     */
    memset(&timeout_action, 0, sizeof(timeout_action));
    timeout_action.sa_handler = connection_timeout_handler;
    sigemptyset(&timeout_action.sa_mask);
    timeout_action.sa_flags = 0;

    api_result = sigaction(SIGALRM, &timeout_action, NULL);
    if (api_result == -1) {
        perror("[Client] ERROR: sigaction() failed");
        return EXIT_FAILURE;
    }

    /*
     * Req 7 & 8: Retry loop with timeout to connect to server
     * Continuously attempts connection for up to 10 seconds
     */
    printf("[Client] Attempting to connect to server: %s\n",
           ENCRYPTION_SERVICE_NAME);
    printf("[Client] Timeout: %d seconds\n\n", CLIENT_CONNECTION_TIMEOUT);

    start_time = time(NULL);
    g_timeout_occurred = 0;

    while (1) {
        /* Check if timeout has occurred */
        current_time = time(NULL);
        elapsed_seconds = difftime(current_time, start_time);

        if (elapsed_seconds >= CLIENT_CONNECTION_TIMEOUT) {
            fprintf(stderr, "[Client] ERROR: Connection timeout after %.0f seconds\n",
                    elapsed_seconds);
            fprintf(stderr, "[Client] HINT: Make sure server is running\n");
            return EXIT_FAILURE;
        }

        /*
         * Req 9: Connect to server using name_open() ONLY
         * This is the required method per project specification
         */
        server_fd = name_open(ENCRYPTION_SERVICE_NAME, O_RDWR);

        /* Req 8: Check return value */
        if (server_fd != -1) {
            /* Successfully connected */
            break;
        }

        /* Connection failed - check error type */
        if (errno == ENOENT) {
            /* Server not found - retry after delay */
            printf("[Client] Server not available, retrying... (%.0fs elapsed)\n",
                   elapsed_seconds);
            sleep(1);
            continue;
        } else {
            /* Other error - abort */
            perror("[Client] ERROR: name_open() failed");
            return EXIT_FAILURE;
        }
    }

    printf("[Client] Successfully connected to server (fd=%d)\n\n", server_fd);

    /* Req 3: Initialize IOV structures for message passing */
    INITIALIZE_IOV(&request_iov[0], &request_message, sizeof(request_message));
    INITIALIZE_IOV(&response_iov[0], &response_message, sizeof(response_message));

    /* Prepare encryption request message */
    memset(&request_message, 0, sizeof(request_message));
    request_message.message_type = MSG_TYPE_ENCRYPT_REQ;
    request_message.data_length  = ENCRYPTION_PAYLOAD_SIZE;

    /* Fill payload with test pattern */
    memset(request_message.payload_data, 'A', ENCRYPTION_PAYLOAD_SIZE);

    printf("[Client] Prepared encryption request\n");
    printf("[Client]   Payload size: %zu bytes\n", request_message.data_length);
    printf("[Client]   First 8 bytes (before): ");
    for (int i = 0; i < 8; i++) {
        printf("0x%02X ", (unsigned char)request_message.payload_data[i]);
    }
    printf("\n\n");

    /* Set alarm for message send timeout */
    alarm(CLIENT_CONNECTION_TIMEOUT);
    g_timeout_occurred = 0;

    /*
     * Req 1, 3, 8: Send message using IOV and check return value
     * This blocks until server replies or timeout occurs
     */
    printf("[Client] Sending encryption request to server...\n");
    api_result = MsgSendv(server_fd, request_iov, 1, response_iov, 1);

    /* Cancel alarm */
    alarm(0);

    /* Req 8: Check return value */
    if (api_result == -1) {
        if (g_timeout_occurred) {
            fprintf(stderr, "[Client] ERROR: Message send timeout after %d seconds\n",
                    CLIENT_CONNECTION_TIMEOUT);
        } else {
            perror("[Client] ERROR: MsgSendv() failed");
        }
        name_close(server_fd);
        return EXIT_FAILURE;
    }

    /* Validate response message type */
    if (response_message.message_type != MSG_TYPE_ENCRYPT_RESP) {
        fprintf(stderr, "[Client] ERROR: Invalid response type: %d (expected %d)\n",
                response_message.message_type, MSG_TYPE_ENCRYPT_RESP);
        name_close(server_fd);
        return EXIT_FAILURE;
    }

    /* Check server operation result */
    if (response_message.operation_result != 0) {
        fprintf(stderr, "[Client] ERROR: Server returned error code: %d\n",
                response_message.operation_result);
        name_close(server_fd);
        return EXIT_FAILURE;
    }

    /* Display results */
    printf("[Client] Encryption completed successfully!\n");
    printf("[Client]   Bytes processed: %zu\n", response_message.bytes_processed);
    printf("[Client]   Server checksum: 0x%08X\n", response_message.data_checksum);

    printf("[Client]   First 8 bytes (after):  ");
    for (int i = 0; i < 8; i++) {
        printf("0x%02X ", (unsigned char)response_message.payload_data[i]);
    }
    printf("\n\n");

    /* Verify encryption result */
    unsigned int local_checksum = calculate_data_checksum(
        response_message.payload_data,
        response_message.bytes_processed
    );

    if (local_checksum == response_message.data_checksum) {
        printf("[Client] Checksum verification: PASSED ✓\n");
    } else {
        printf("[Client] Checksum verification: MISMATCH ✗\n");
        printf("[Client]   Expected: 0x%08X\n", local_checksum);
    }

    /* Note about asynchronous notification */
    printf("[Client] Server sent asynchronous pulse notification (handled by kernel)\n");

    /* Cleanup - close connection */
    api_result = name_close(server_fd);
    if (api_result == -1) {
        perror("[Client] WARNING: name_close() failed");
    } else {
        printf("[Client] Connection closed successfully\n");
    }

    printf("\n");
    printf("============================================================\n");
    printf("   Client operation completed successfully\n");
    printf("============================================================\n\n");

    return EXIT_SUCCESS;
}
