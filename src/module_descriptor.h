#ifndef MODULE_DESCRIPTOR_H
#define MODULE_DESCRIPTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct {
    char module_name[64];
    char uuid[64];               // UUIDv4
    char semantic_profile[128];  // 建議放 JSON，例如 {"type":"AGI","sub":"SVM"}
    int  supports_blockchain;    // 0/1
} RetryIXModuleDescriptor;

// 註冊目前 Host 模組（由 retryix_host.c 內建簡易實作）
int retryix_register_module(const RetryIXModuleDescriptor* desc);

// 查詢模組資訊（JSON 字串輸出到 buffer）
int retryix_query_module(char* buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // MODULE_DESCRIPTOR_H
