#ifndef HOST_COMM_H
#define HOST_COMM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

#define MAX_MSG_SIZE  4096
#define MAX_MSG_QUEUE 64

typedef enum {
    COMM_SUCCESS = 0,
    COMM_ERROR_SEND = -1,
    COMM_ERROR_RECV = -2,
    COMM_ERROR_QUEUE_FULL = -3,
    COMM_ERROR_INIT = -4
} comm_result_t;

typedef struct {
    uint32_t msg_id;
    uint32_t payload_size;
    uint8_t  payload[MAX_MSG_SIZE];
} comm_packet_t;

// 初始化通訊模組（此版本為單程序 queue 模擬，可後續替換為 SVM/共享記憶體/P2P）
comm_result_t comm_init(const char* channel_name);

// 發送封包（非阻塞）
comm_result_t comm_send(const comm_packet_t* packet);

// 接收封包（非阻塞，若無資料回傳 COMM_ERROR_RECV）
comm_result_t comm_recv(comm_packet_t* out_packet);

// 清理通訊資源
void comm_cleanup();

#ifdef __cplusplus
}
#endif

#endif // HOST_COMM_H
