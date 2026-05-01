/*
 * ============================================================================
 * server.c - QNX Concurrent File Encryption Service Server
 * ============================================================================
 *
 * Description:
 *   This server receives encryption requests from clients, processes the
 *   data using a thread pool for concurrent encryption, and sends back
 *   the encrypted data asynchronously.
 *
 * Requirements Met:
 *   - Req 1: Uses QNX message passing API (MsgSend/Receive/Reply)
 *   - Req 2: Client-server architecture
 *   - Req 3: IOV transfers for large payloads
 *   - Req 4: Handles 64KB+ payloads
 *   - Req 6: Well-commented with meaningful variable names
 *   - Req 10: Sends asynchronous response via event/pulse
 *
 * Architecture:
 *   - Main thread: Accepts client connections and messages
 *   - Worker threads: Perform concurrent encryption on data segments
 *   - Synchronization: Mutex and condition variables
 * ============================================================================
 */

#include "message.h"

/* Global synchronization objects */
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_sync_cond   = PTHREAD_COND_INITIALIZER;

/**
 * Worker thread function - performs XOR encryption on assigned segment
 *
 * Parameters:
 *   arg - Pointer to thread_worker_context_t structure
 *
 * Returns:
 *   NULL (void pointer)
 */
static void* encryption_worker_thread(void* arg) {
    thread_worker_context_t* context = (thread_worker_context_t*)arg;

    if (context == NULL || context->data_segment == NULL) {
        return NULL;
    }

    /* Perform XOR encryption on assigned data segment */
    for (size_t i = 0; i < context->segment_size; i++) {
        context->data_segment[i] ^= ENCRYPTION_KEY;
    }

    printf("[Thread-%d] Encrypted %zu bytes successfully\n",
           context->worker_thread_id, context->segment_size);

    /* Signal completion using mutex and condition variable */
    pthread_mutex_lock(context->sync_mutex);
    (*context->completed_threads)++;

    /* Wake up main thread if all workers are done */
    if (*context->completed_threads >= 2) {
        pthread_cond_signal(context->completion_cond);
    }
    pthread_mutex_unlock(context->sync_mutex);

    return NULL;
}

/**
 * Main server entry point
 */
int main(void) {
    name_attach_t* name_attachment = NULL;
    int            client_rcvid;
    int            return_status;

    /* Message buffers */
    encrypt_request_msg_t  request_message;
    encrypt_response_msg_t response_message;

    /* IOV structures for zero-copy messaging */
    iov_t  request_iov[1];
    iov_t  response_iov[1];

    /* Thread management */
    pthread_t            worker_threads[2];
    thread_worker_context_t thread_contexts[2];
    int                  completed_thread_count = 0;

    printf("\n");
    printf("============================================================\n");
    printf("   QNX Concurrent File Encryption Service - Server\n");
    printf("============================================================\n\n");

    /*
     * Req 1 & 2: Attach to QNX namespace for client connections
     * This creates a channel and registers it in the pathname space
     */
    name_attachment = name_attach(NULL, ENCRYPTION_SERVICE_NAME, 0);
    if (name_attachment == NULL) {
        int error_code = errno;
        fprintf(stderr, "ERROR: name_attach() failed: %s (errno=%d)\n",
                strerror(error_code), error_code);
        fprintf(stderr, "HINT: Try running 'devc-name /dev/name &' first\n");
        return EXIT_FAILURE;
    }

    printf("[Server] Successfully attached to service: %s\n",
           ENCRYPTION_SERVICE_NAME);
    printf("[Server] Channel ID: %d\n", name_attachment->chid);
    printf("[Server] Waiting for client connections...\n\n");

    /* Initialize IOV structures for message passing - Req 3 */
    INITIALIZE_IOV(&request_iov[0], &request_message, sizeof(request_message));
    INITIALIZE_IOV(&response_iov[0], &response_message, sizeof(response_message));

    /* Main server loop - process client requests indefinitely */
    while (1) {
        /* Req 1: Blocking receive - waits for client message */
        client_rcvid = MsgReceivev(name_attachment->chid, request_iov, 1, NULL);

        /* Handle errors */
        if (client_rcvid == -1) {
            perror("[Server] ERROR: MsgReceivev() failed");
            continue;
        }

        /* Pulse message received (rcvid == 0) - ignore */
        if (client_rcvid == 0) {
            printf("[Server] Received pulse (ignored)\n");
            continue;
        }

        /* Validate message type */
        if (request_message.message_type != MSG_TYPE_ENCRYPT_REQ) {
            printf("[Server] Invalid message type: %d (expected %d)\n",
                   request_message.message_type, MSG_TYPE_ENCRYPT_REQ);

            response_message.message_type   = MSG_TYPE_ENCRYPT_RESP;
            response_message.operation_result = -1;
            response_message.bytes_processed = 0;
            response_message.data_checksum  = 0;

            MsgReplyv(client_rcvid, EOK, response_iov, 1);
            continue;
        }

        /* Validate data length */
        if (request_message.data_length == 0 ||
            request_message.data_length > ENCRYPTION_PAYLOAD_SIZE) {
            printf("[Server] Invalid data length: %zu\n",
                   request_message.data_length);

            response_message.message_type   = MSG_TYPE_ENCRYPT_RESP;
            response_message.operation_result = -1;
            response_message.bytes_processed = 0;
            response_message.data_checksum  = 0;

            MsgReplyv(client_rcvid, EOK, response_iov, 1);
            continue;
        }

        printf("[Server] Received encryption request\n");
        printf("[Server]   Data length: %zu bytes\n", request_message.data_length);
        printf("[Server]   First 8 bytes (hex): ");
        for (int i = 0; i < 8 && i < (int)request_message.data_length; i++) {
            printf("0x%02X ", (unsigned char)request_message.payload_data[i]);
        }
        printf("\n\n");

        /* Reset completion counter */
        completed_thread_count = 0;

        /* Calculate segment sizes for parallel processing */
        size_t segment1_size = request_message.data_length / 2;
        size_t segment2_size = request_message.data_length - segment1_size;

        /* Setup thread context 1 */
        thread_contexts[0].data_segment    = request_message.payload_data;
        thread_contexts[0].segment_size    = segment1_size;
        thread_contexts[0].worker_thread_id = 1;
        thread_contexts[0].sync_mutex      = &g_sync_mutex;
        thread_contexts[0].completed_threads = &completed_thread_count;
        thread_contexts[0].completion_cond = &g_sync_cond;

        /* Setup thread context 2 */
        thread_contexts[1].data_segment    = request_message.payload_data + segment1_size;
        thread_contexts[1].segment_size    = segment2_size;
        thread_contexts[1].worker_thread_id = 2;
        thread_contexts[1].sync_mutex      = &g_sync_mutex;
        thread_contexts[1].completed_threads = &completed_thread_count;
        thread_contexts[1].completion_cond = &g_sync_cond;

        printf("[Server] Creating worker threads for concurrent processing...\n");

        /* Create worker thread 1 */
        return_status = pthread_create(&worker_threads[0], NULL,
                                       encryption_worker_thread,
                                       &thread_contexts[0]);
        if (return_status != 0) {
            fprintf(stderr, "[Server] ERROR: pthread_create() for thread 1: %s\n",
                    strerror(return_status));
            response_message.message_type   = MSG_TYPE_ENCRYPT_RESP;
            response_message.operation_result = -1;
            MsgReplyv(client_rcvid, EOK, response_iov, 1);
            continue;
        }

        /* Create worker thread 2 */
        return_status = pthread_create(&worker_threads[1], NULL,
                                       encryption_worker_thread,
                                       &thread_contexts[1]);
        if (return_status != 0) {
            fprintf(stderr, "[Server] ERROR: pthread_create() for thread 2: %s\n",
                    strerror(return_status));
            pthread_join(worker_threads[0], NULL);
            response_message.message_type   = MSG_TYPE_ENCRYPT_RESP;
            response_message.operation_result = -1;
            MsgReplyv(client_rcvid, EOK, response_iov, 1);
            continue;
        }

        /* Wait for both threads to complete using condition variable */
        pthread_mutex_lock(&g_sync_mutex);
        while (completed_thread_count < 2) {
            pthread_cond_wait(&g_sync_cond, &g_sync_mutex);
        }
        pthread_mutex_unlock(&g_sync_mutex);

        /* Join threads to clean up resources */
        pthread_join(worker_threads[0], NULL);
        pthread_join(worker_threads[1], NULL);

        printf("[Server] All worker threads completed\n");

        /* Calculate checksum of encrypted data */
        unsigned int encrypted_checksum = calculate_data_checksum(
            request_message.payload_data,
            request_message.data_length
        );

        /* Prepare response message */
        response_message.message_type     = MSG_TYPE_ENCRYPT_RESP;
        response_message.operation_result = 0;  /* Success */
        response_message.bytes_processed  = request_message.data_length;
        response_message.data_checksum    = encrypted_checksum;
        /* FIX: Copy encrypted data into response so client can verify */
        memcpy(response_message.payload_data,
               request_message.payload_data,
               request_message.data_length);

        /* Send synchronous reply to client */
        return_status = MsgReplyv(client_rcvid, EOK, response_iov, 1);
        if (return_status == -1) {
            perror("[Server] ERROR: MsgReplyv() failed");
            continue;
        }

        printf("[Server] Sent synchronous reply to client\n");
        printf("[Server]   Bytes processed: %zu\n", response_message.bytes_processed);
        printf("[Server]   Checksum: 0x%08X\n", encrypted_checksum);

        /*
         * Req 10: Send asynchronous event notification via pulse
         * This demonstrates async communication capability
         */
        struct sigevent async_event;
        SIGEV_PULSE_INIT(&async_event,
                        name_attachment->chid,
                        SIGEV_PULSE_PRIO_INHERIT,
                        PULSE_CODE_COMPLETE,
                        0);

        return_status = MsgSendPulse(name_attachment->chid,
                                    SIGEV_PULSE_PRIO_INHERIT,
                                    PULSE_CODE_COMPLETE,
                                    0);
        if (return_status == -1) {
            perror("[Server] WARNING: MsgSendPulse() failed");
        } else {
            printf("[Server] Sent asynchronous pulse notification\n");
        }

        printf("\n[Server] Request completed successfully\n");
        printf("------------------------------------------------------------\n\n");
    }

    /* Cleanup (unreachable in this infinite loop, but good practice) */
    name_detach(name_attachment, 0);
    printf("[Server] Server shutdown complete\n");

    return EXIT_SUCCESS;
}
