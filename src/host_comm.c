#include "host_comm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static comm_packet_t msg_queue[MAX_MSG_QUEUE];
static int queue_head = 0;
static int queue_tail = 0;
static int initialized = 0;

comm_result_t comm_init(const char* channel_name) {
    (void)channel_name; // 預留：可用於命名命名管道/共享記憶體
    if (initialized) return COMM_SUCCESS;
    memset(msg_queue, 0, sizeof(msg_queue));
    queue_head = queue_tail = 0;
    initialized = 1;
    return COMM_SUCCESS;
}

comm_result_t comm_send(const comm_packet_t* packet) {
    if (!initialized) return COMM_ERROR_INIT;
    if (((queue_tail + 1) % MAX_MSG_QUEUE) == queue_head)
        return COMM_ERROR_QUEUE_FULL;

    msg_queue[queue_tail] = *packet;
    queue_tail = (queue_tail + 1) % MAX_MSG_QUEUE;
    return COMM_SUCCESS;
}

comm_result_t comm_recv(comm_packet_t* out_packet) {
    if (!initialized) return COMM_ERROR_INIT;
    if (queue_head == queue_tail) return COMM_ERROR_RECV;

    *out_packet = msg_queue[queue_head];
    queue_head = (queue_head + 1) % MAX_MSG_QUEUE;
    return COMM_SUCCESS;
}

void comm_cleanup() {
    initialized = 0;
    queue_head = queue_tail = 0;
}
