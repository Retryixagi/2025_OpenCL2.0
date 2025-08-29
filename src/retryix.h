#ifndef RETRYIX_H
#define RETRYIX_H

#include <stddef.h>
#include <CL/cl.h>

#ifdef _WIN32
#define RETRYIX_API __declspec(dllexport)
#else
#define RETRYIX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// === 資源總覽查詢 API ===
RETRYIX_API int retryix_query_all_resources(char* json_out, size_t max_len);

// RetryIX 設備結構體
typedef struct {
    char name[256];
    cl_device_type type;
    cl_ulong global_mem_size;
    char opencl_version[128];
    char extensions[2048];
    cl_bitfield svm_capabilities;
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
int retryixGetPlatformList(cl_platform_id *platforms, cl_uint max, cl_uint *num_found);
int retryixGetDeviceList(RetryIXDevice* devices, int max_devices, int* out_count);
int retryixGetDevicesForPlatform(cl_platform_id platform, RetryIXDevice* devices, int max_devices, int* out_count);

// === 實用工具 API ===
int retryix_export_all_devices_json(const char* out_path);
int retryixFindDeviceById(cl_device_id device_id, RetryIXDevice* out_device);
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

// === Kernel 測試與管理 API ===
RETRYIX_API void retryix_kernel_atomic_add_demo(void);
int retryix_kernel_register_template(const char* template_name, const char* kernel_name, const char* source_code);
int retryix_kernel_execute(const char* template_name, size_t global_work_size, size_t local_work_size, ...);

#ifdef __cplusplus
}
#endif

#endif // RETRYIX_H