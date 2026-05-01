/*
 * ============================================================================
 * messages.h - Common Message Definitions for QNX Encryption Service
 * ============================================================================
 * This header file is shared between client and server processes.
 * It defines message structures, constants, and helper macros.
 *
 * Requirements Met:
 * - Req 5: Message definitions in common header file
 * - Req 4: Payload size >= 64KB
 * - Req 6: Well-documented with meaningful names
 * ============================================================================
 */

#ifndef MESSAGES_H
#define MESSAGES_H

/* Standard QNX and POSIX headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/* ============================================================================
 * Configuration Constants
 * ============================================================================ */

/* Req 9: Service name for name_open/name_attach */
#define ENCRYPTION_SERVICE_NAME    "qnx_encrypt_svc"

/* Req 4: Payload size must be at least 64KB */
#define ENCRYPTION_PAYLOAD_SIZE    (64 * 1024 + 2048)  /* 66KB total */

/* Req 7: Client timeout in seconds */
#define CLIENT_CONNECTION_TIMEOUT  10

/* Message type identifiers */
#define MSG_TYPE_ENCRYPT_REQ       1
#define MSG_TYPE_ENCRYPT_RESP      2
#define PULSE_CODE_COMPLETE        100

/* Encryption key (XOR cipher for demonstration) */
#define ENCRYPTION_KEY             0xAA

/* ============================================================================
 * Message Structures
 * ============================================================================ */

/**
 * Client-to-Server encryption request message
 *
 * Members:
 *   message_type    - Identifies this as an encryption request
 *   data_length     - Actual number of bytes in payload to encrypt
 *   payload_data    - Buffer containing data to be encrypted (64KB+)
 */
typedef struct {
    int     message_type;
    size_t  data_length;
    char    payload_data[ENCRYPTION_PAYLOAD_SIZE];
} encrypt_request_msg_t;

/**
 * Server-to-Client encryption response message
 *
 * Members:
 *   message_type      - Identifies this as an encryption response
 *   operation_result  - 0 = success, -1 = error
 *   bytes_processed   - Number of bytes successfully encrypted
 *   data_checksum     - Checksum of encrypted data for verification
 */
typedef struct {
    int           message_type;
    int           operation_result;
    size_t        bytes_processed;
    unsigned int  data_checksum;
    /* NEW: Buffer to hold the encrypted data sent back to client */
    char          payload_data[ENCRYPTION_PAYLOAD_SIZE];
} encrypt_response_msg_t;

/**
 * Thread worker context for concurrent encryption
 *
 * Members:
 *   data_segment        - Pointer to this thread's data segment
 *   segment_size        - Size of the data segment
 *   worker_thread_id    - Unique thread identifier
 *   sync_mutex          - Mutex for thread synchronization
 *   completed_threads   - Counter for completed threads
 *   completion_cond     - Condition variable for signaling
 */
typedef struct {
    char*            data_segment;
    size_t           segment_size;
    int              worker_thread_id;
    pthread_mutex_t* sync_mutex;
    int*             completed_threads;
    pthread_cond_t*  completion_cond;
} thread_worker_context_t;

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * Initialize IOV structure for message passing
 * Req 3: IOV transfer support
 */
#define INITIALIZE_IOV(iov_struct, data_ptr, data_size)  \
    do {                                                  \
        (iov_struct)->iov_base = (void*)(data_ptr);      \
        (iov_struct)->iov_len = (data_size);             \
    } while (0)

/**
 * Calculate simple checksum for data verification
 */
static inline unsigned int calculate_data_checksum(const char* data, size_t length) {
    unsigned int checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum += (unsigned char)data[i];
    }
    return checksum;
}

#endif /* MESSAGES_H */
