// retryix_svm.c - RetryIX SVM Implementation (Fixed)
#define CL_TARGET_OPENCL_VERSION 200  // 避免 OpenCL 3.0 警告

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>  // 添加 aligned_alloc 支援
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
    #include <malloc.h>  // Windows aligned allocation
    #define RETRYIX_EXPORT __declspec(dllexport)
    // Windows 沒有 aligned_alloc，使用 _aligned_malloc
    #define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
    #define aligned_free(ptr) _aligned_free(ptr)
#else
    #define RETRYIX_EXPORT __attribute__((visibility("default")))
    #define aligned_free(ptr) free(ptr)
#endif

// SVM 能力等級定義 - 修正枚舉衝突
typedef enum {
    RETRYIX_SVM_LEVEL_NONE = 0,           // 不支援 SVM
    RETRYIX_SVM_LEVEL_COARSE_GRAIN,       // 粗粒度 SVM (手動同步)
    RETRYIX_SVM_LEVEL_FINE_GRAIN,         // 細粒度 SVM (自動同步)
    RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM,  // 系統級細粒度 SVM
    RETRYIX_SVM_LEVEL_EMULATED            // 軟體模擬 SVM
} retryix_svm_level_t;

// SVM 記憶體標誌 - 與等級分開定義
typedef enum {
    RETRYIX_SVM_FLAG_READ_WRITE = 0x1,
    RETRYIX_SVM_FLAG_READ_ONLY = 0x2,
    RETRYIX_SVM_FLAG_WRITE_ONLY = 0x4,
    RETRYIX_SVM_FLAG_ATOMIC = 0x8,
    RETRYIX_SVM_FLAG_FINE_GRAIN = 0x10,
    RETRYIX_SVM_FLAG_COARSE_GRAIN = 0x20
} retryix_svm_flags_t;

// SVM 記憶體描述符
typedef struct {
    void* ptr;                      // SVM 指針
    size_t size;                   // 記憶體大小
    retryix_svm_flags_t flags;     // SVM 標誌
    retryix_svm_level_t level;     // SVM 等級
    cl_context context;            // 關聯的 OpenCL 上下文
    cl_device_id device;           // 關聯設備
    uint32_t ref_count;            // 引用計數
    bool is_mapped;                // 是否已映射
    void* fallback_buffer;         // 回退緩衝區（用於不支援 SVM 的設備）
    cl_mem fallback_mem;           // 回退 OpenCL 記憶體對象
} retryix_svm_descriptor_t;

// SVM 上下文管理
typedef struct {
    cl_context context;
    cl_device_id device;
    retryix_svm_level_t max_svm_level;
    cl_bitfield svm_capabilities;
    bool supports_atomic_svm;
    size_t svm_alignment;
    size_t max_svm_size;
    
    // 記憶體管理
    retryix_svm_descriptor_t* descriptors;
    size_t descriptor_count;
    size_t descriptor_capacity;
    
    // 統計資訊
    size_t total_allocated;
    size_t peak_allocated;
    uint64_t alloc_count;
    uint64_t free_count;
} retryix_svm_context_t;

// === 函數聲明 ===
RETRYIX_EXPORT retryix_svm_context_t* retryix_svm_create_context(cl_context context, cl_device_id device);
RETRYIX_EXPORT void retryix_svm_destroy_context(retryix_svm_context_t* svm_ctx);
RETRYIX_EXPORT retryix_svm_level_t retryix_svm_probe_capabilities(cl_device_id device, cl_bitfield* capabilities);
RETRYIX_EXPORT void* retryix_svm_alloc(retryix_svm_context_t* ctx, size_t size, retryix_svm_flags_t flags);
RETRYIX_EXPORT int retryix_svm_free(retryix_svm_context_t* ctx, void* ptr);
RETRYIX_EXPORT int retryix_svm_map(retryix_svm_context_t* ctx, void* ptr, cl_command_queue queue);
RETRYIX_EXPORT int retryix_svm_unmap(retryix_svm_context_t* ctx, void* ptr, cl_command_queue queue);

// 檢測設備 SVM 能力的實現
retryix_svm_level_t retryix_svm_probe_capabilities(cl_device_id device, cl_bitfield* capabilities) {
    char extensions[4096] = {0};
    char version[256] = {0};
    cl_bitfield svm_caps = 0;
    
    if (!device || !capabilities) {
        *capabilities = 0;
        return RETRYIX_SVM_LEVEL_NONE;
    }
    
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(extensions), extensions, NULL);
    clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(version), version, NULL);
    
    // 檢查 OpenCL 2.0+ 支援
    int major = 0, minor = 0;
    if (sscanf(version, "OpenCL %d.%d", &major, &minor) == 2 && major >= 2) {
        // 嘗試查詢 SVM 能力
        cl_int err = clGetDeviceInfo(device, CL_DEVICE_SVM_CAPABILITIES, sizeof(svm_caps), &svm_caps, NULL);
        
        if (err == CL_SUCCESS && svm_caps != 0) {
            *capabilities = svm_caps;
            
            // 根據能力等級分類
            if (svm_caps & CL_DEVICE_SVM_FINE_GRAIN_SYSTEM) {
                return RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM;
            } else if (svm_caps & CL_DEVICE_SVM_FINE_GRAIN_BUFFER) {
                return RETRYIX_SVM_LEVEL_FINE_GRAIN;
            } else if (svm_caps & CL_DEVICE_SVM_COARSE_GRAIN_BUFFER) {
                return RETRYIX_SVM_LEVEL_COARSE_GRAIN;
            }
        }
    }
    
    // 回退到軟體模擬 SVM
    *capabilities = 0;
    return RETRYIX_SVM_LEVEL_EMULATED;
}

// 創建 SVM 上下文
retryix_svm_context_t* retryix_svm_create_context(cl_context context, cl_device_id device) {
    if (!context || !device) return NULL;
    
    retryix_svm_context_t* ctx = (retryix_svm_context_t*)calloc(1, sizeof(retryix_svm_context_t));
    if (!ctx) return NULL;
    
    ctx->context = context;
    ctx->device = device;
    ctx->max_svm_level = retryix_svm_probe_capabilities(device, &ctx->svm_capabilities);
    ctx->supports_atomic_svm = (ctx->svm_capabilities & CL_DEVICE_SVM_ATOMICS) != 0;
    
    // 查詢 SVM 對齊要求
    size_t alignment = 0;
    clGetDeviceInfo(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(alignment), &alignment, NULL);
    ctx->svm_alignment = (alignment > 0) ? (alignment / 8) : 64; // 轉換為位元組，預設64位元組
    
    // 查詢最大 SVM 大小
    cl_ulong max_alloc_size = 0;
    clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(max_alloc_size), &max_alloc_size, NULL);
    ctx->max_svm_size = (size_t)max_alloc_size;
    
    // 初始化描述符陣列
    ctx->descriptor_capacity = 64;
    ctx->descriptors = (retryix_svm_descriptor_t*)calloc(ctx->descriptor_capacity, sizeof(retryix_svm_descriptor_t));
    if (!ctx->descriptors) {
        free(ctx);
        return NULL;
    }
    
    printf("RetryIX SVM Context Created\n");
    printf("  SVM Level: %d\n", ctx->max_svm_level);
    printf("  Capabilities: 0x%08x\n", (unsigned)ctx->svm_capabilities);
    printf("  Atomic Support: %s\n", ctx->supports_atomic_svm ? "YES" : "NO");
    printf("  Alignment: %zu bytes\n", ctx->svm_alignment);
    printf("  Max SVM Size: %.2f MB\n", (double)ctx->max_svm_size / (1024*1024));
    
    return ctx;
}

// 通用 SVM 分配實現
void* retryix_svm_alloc(retryix_svm_context_t* ctx, size_t size, retryix_svm_flags_t flags) {
    if (!ctx || size == 0) return NULL;
    
    // 對齊大小
    size_t aligned_size = (size + ctx->svm_alignment - 1) & ~(ctx->svm_alignment - 1);
    
    void* ptr = NULL;
    retryix_svm_descriptor_t desc = {0};
    desc.size = aligned_size;
    desc.flags = flags;
    desc.context = ctx->context;
    desc.device = ctx->device;
    desc.ref_count = 1;
    
    // 根據設備能力選擇分配策略
    switch (ctx->max_svm_level) {
        case RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM:
        case RETRYIX_SVM_LEVEL_FINE_GRAIN:
        case RETRYIX_SVM_LEVEL_COARSE_GRAIN: {
            // 使用原生 SVM 分配
            cl_svm_mem_flags svm_flags = CL_MEM_READ_WRITE;
            
            if (ctx->max_svm_level >= RETRYIX_SVM_LEVEL_FINE_GRAIN) {
                svm_flags |= CL_MEM_SVM_FINE_GRAIN_BUFFER;
            }
            if (ctx->supports_atomic_svm && (flags & RETRYIX_SVM_FLAG_ATOMIC)) {
                svm_flags |= CL_MEM_SVM_ATOMICS;
            }
            
            ptr = clSVMAlloc(ctx->context, svm_flags, aligned_size, ctx->svm_alignment);
            desc.level = ctx->max_svm_level;
            desc.is_mapped = true;
            
            if (ptr) {
                printf("Native SVM allocation: %p (%zu bytes, level %d)\n", ptr, aligned_size, desc.level);
            }
            break;
        }
        
        case RETRYIX_SVM_LEVEL_EMULATED:
        case RETRYIX_SVM_LEVEL_NONE:
        default: {
            // 軟體模擬 SVM（使用標準記憶體 + OpenCL 緩衝區）
            ptr = aligned_alloc(ctx->svm_alignment, aligned_size);
            if (ptr) {
                // 創建對應的 OpenCL 緩衝區
                cl_int err;
                desc.fallback_mem = clCreateBuffer(ctx->context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                                  aligned_size, ptr, &err);
                if (err != CL_SUCCESS) {
                    aligned_free(ptr);
                    ptr = NULL;
                } else {
                    desc.fallback_buffer = ptr;
                    desc.level = RETRYIX_SVM_LEVEL_EMULATED;
                    desc.is_mapped = false;
                    printf("Emulated SVM allocation: %p (%zu bytes)\n", ptr, aligned_size);
                }
            }
            break;
        }
    }
    
    if (ptr) {
        // 添加到描述符陣列
        if (ctx->descriptor_count >= ctx->descriptor_capacity) {
            ctx->descriptor_capacity *= 2;
            ctx->descriptors = (retryix_svm_descriptor_t*)realloc(ctx->descriptors, 
                                ctx->descriptor_capacity * sizeof(retryix_svm_descriptor_t));
        }
        
        desc.ptr = ptr;
        ctx->descriptors[ctx->descriptor_count++] = desc;
        
        // 更新統計
        ctx->total_allocated += aligned_size;
        ctx->alloc_count++;
        if (ctx->total_allocated > ctx->peak_allocated) {
            ctx->peak_allocated = ctx->total_allocated;
        }
    }
    
    return ptr;
}

// 釋放 SVM 記憶體
int retryix_svm_free(retryix_svm_context_t* ctx, void* ptr) {
    if (!ctx || !ptr) return -1;
    
    // 查找描述符
    for (size_t i = 0; i < ctx->descriptor_count; i++) {
        if (ctx->descriptors[i].ptr == ptr) {
            retryix_svm_descriptor_t* desc = &ctx->descriptors[i];
            
            // 減少引用計數
            if (--desc->ref_count > 0) {
                return 0; // 仍有其他引用
            }
            
            // 根據類型釋放記憶體
            switch (desc->level) {
                case RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM:
                case RETRYIX_SVM_LEVEL_FINE_GRAIN:
                case RETRYIX_SVM_LEVEL_COARSE_GRAIN:
                    clSVMFree(ctx->context, ptr);
                    printf("Native SVM freed: %p\n", ptr);
                    break;
                    
                case RETRYIX_SVM_LEVEL_EMULATED:
                case RETRYIX_SVM_LEVEL_NONE:
                default:
                    if (desc->fallback_mem) {
                        clReleaseMemObject(desc->fallback_mem);
                    }
                    if (desc->fallback_buffer) {
                        aligned_free(desc->fallback_buffer);
                    }
                    printf("Emulated SVM freed: %p\n", ptr);
                    break;
            }
            
            // 更新統計
            ctx->total_allocated -= desc->size;
            ctx->free_count++;
            
            // 從陣列中移除（交換到末尾）
            ctx->descriptors[i] = ctx->descriptors[--ctx->descriptor_count];
            return 0;
        }
    }
    
    return -1; // 未找到
}

// SVM 記憶體映射
int retryix_svm_map(retryix_svm_context_t* ctx, void* ptr, cl_command_queue queue) {
    if (!ctx || !ptr || !queue) return -1;
    
    // 查找描述符
    for (size_t i = 0; i < ctx->descriptor_count; i++) {
        if (ctx->descriptors[i].ptr == ptr) {
            retryix_svm_descriptor_t* desc = &ctx->descriptors[i];
            
            if (desc->is_mapped) return 0; // 已經映射
            
            cl_int err = CL_SUCCESS;
            switch (desc->level) {
                case RETRYIX_SVM_LEVEL_COARSE_GRAIN:
                    // 粗粒度 SVM 需要顯式映射
                    err = clEnqueueSVMMap(queue, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, ptr, desc->size, 0, NULL, NULL);
                    break;
                    
                case RETRYIX_SVM_LEVEL_FINE_GRAIN:
                case RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM:
                    // 細粒度 SVM 自動映射
                    break;
                    
                case RETRYIX_SVM_LEVEL_EMULATED:
                case RETRYIX_SVM_LEVEL_NONE:
                default:
                    // 模擬 SVM 不需要映射
                    break;
            }
            
            if (err == CL_SUCCESS) {
                desc->is_mapped = true;
                return 0;
            }
            return -1;
        }
    }
    
    return -1;
}

// SVM 記憶體解映射
int retryix_svm_unmap(retryix_svm_context_t* ctx, void* ptr, cl_command_queue queue) {
    if (!ctx || !ptr || !queue) return -1;
    
    // 查找描述符
    for (size_t i = 0; i < ctx->descriptor_count; i++) {
        if (ctx->descriptors[i].ptr == ptr) {
            retryix_svm_descriptor_t* desc = &ctx->descriptors[i];
            
            if (!desc->is_mapped) return 0; // 已經解映射
            
            cl_int err = CL_SUCCESS;
            switch (desc->level) {
                case RETRYIX_SVM_LEVEL_COARSE_GRAIN:
                    // 粗粒度 SVM 需要顯式解映射
                    err = clEnqueueSVMUnmap(queue, ptr, 0, NULL, NULL);
                    break;
                    
                case RETRYIX_SVM_LEVEL_FINE_GRAIN:
                case RETRYIX_SVM_LEVEL_FINE_GRAIN_SYSTEM:
                    // 細粒度 SVM 自動管理
                    break;
                    
                case RETRYIX_SVM_LEVEL_EMULATED:
                case RETRYIX_SVM_LEVEL_NONE:
                default:
                    // 模擬 SVM：需要顯式拷貝數據
                    if (desc->fallback_mem) {
                        err = clEnqueueWriteBuffer(queue, desc->fallback_mem, CL_TRUE, 0, desc->size, ptr, 0, NULL, NULL);
                    }
                    break;
            }
            
            if (err == CL_SUCCESS) {
                desc->is_mapped = false;
                return 0;
            }
            return -1;
        }
    }
    
    return -1;
}

// 銷毀 SVM 上下文
void retryix_svm_destroy_context(retryix_svm_context_t* svm_ctx) {
    if (!svm_ctx) return;
    
    printf("Destroying RetryIX SVM Context\n");
    printf("  Total Allocations: %llu\n", (unsigned long long)svm_ctx->alloc_count);
    printf("  Total Frees: %llu\n", (unsigned long long)svm_ctx->free_count);
    printf("  Peak Memory: %.2f MB\n", (double)svm_ctx->peak_allocated / (1024*1024));
    
    // 釋放所有未釋放的 SVM 記憶體
    while (svm_ctx->descriptor_count > 0) {
        retryix_svm_free(svm_ctx, svm_ctx->descriptors[0].ptr);
    }
    
    if (svm_ctx->descriptors) {
        free(svm_ctx->descriptors);
    }
    free(svm_ctx);
}