#include "retryix.h"
#include <stdio.h>
#include <string.h>

// TODO: 可用 cJSON 或 jansson 實作更完整的 JSON 輸出
RETRYIX_API int retryix_query_all_resources(char* json_out, size_t max_len) {
    // 目前僅示範，實際可整合 retryix_export_all_devices_json、kernel、SVM、模組、runtime 狀態等
    snprintf(json_out, max_len,
        "{\n"
        "  \"platforms\": [\"stub\"],\n"
        "  \"devices\": [\"stub\"],\n"
        "  \"svm\": {\"supported\": false},\n"
        "  \"kernels\": [\"stub\"],\n"
        "  \"modules\": [\"stub\"],\n"
        "  \"runtime\": {\"status\": \"stub\"}\n"
        "}"
    );
    return 0;
}
