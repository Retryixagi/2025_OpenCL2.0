#ifndef RETRYIX_H
#define RETRYIX_H

#include <CL/cl.h>

#ifdef __cplusplus
extern "C" {
#endif

// RetryIX 設備結構體
typedef struct {
    char name[256];
    cl_device_type type;
    cl_ulong global_mem_size;
    char opencl_version[128];
    char extensions[2048];          // 新增：支援的擴展
    cl_bitfield svm_capabilities;   // 新增：SVM 能力
    cl_device_id id;
} RetryIXDevice;

// RetryIX 平台結構體
typedef struct {
    char name[256];
    char vendor[256];
    char version[128];
    cl_platform_id id;
    int device_count;
} RetryIXPlatform;

// === 核心 API ===

// 平台偵測 API
int retryixGetPlatformList(cl_platform_id *platforms, cl_uint max, cl_uint *num_found);

// 裝置列舉 API（主入口）
int retryixGetDeviceList(RetryIXDevice* devices, int max_devices, int* out_count);

// 取得指定平台的所有裝置
int retryixGetDevicesForPlatform(cl_platform_id platform, RetryIXDevice* devices, 
                                int max_devices, int* out_count);

// === 實用工具 API ===

// 導出所有平台與裝置資訊（含 extension/SVM）成 JSON 檔案
int retryix_export_all_devices_json(const char* out_path);

// 根據設備ID查找設備資訊
int retryixFindDeviceById(cl_device_id device_id, RetryIXDevice* out_device);

// 檢查設備是否支援特定功能
int retryixCheckDeviceCapability(cl_device_id device_id, const char* capability);

// === 錯誤碼定義 ===
#define RETRYIX_SUCCESS          0
#define RETRYIX_ERROR_NULL_PTR  -1
#define RETRYIX_ERROR_NO_DEVICE -2
#define RETRYIX_ERROR_NO_PLATFORM -3
#define RETRYIX_ERROR_OPENCL    -4
#define RETRYIX_ERROR_BUFFER_TOO_SMALL -5
#define RETRYIX_ERROR_FILE_IO   -6

// === 輔助宏 ===
#define RETRYIX_MAX_PLATFORMS   16
#define RETRYIX_MAX_DEVICES     64
#define RETRYIX_MAX_NAME_LEN    256

#ifdef __cplusplus
}
#endif

#endif // RETRYIX_H