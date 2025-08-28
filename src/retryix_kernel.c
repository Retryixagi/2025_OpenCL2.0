
#define CL_TARGET_OPENCL_VERSION 200
#include "retryix_cl_compat.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include "retryix.h"


// ...existing code...

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

// 內核編譯策略
typedef enum {
    RETRYIX_KERNEL_STRATEGY_OPENCL20,      // OpenCL 2.0 原生
    RETRYIX_KERNEL_STRATEGY_OPENCL12_EXT,  // OpenCL 1.2 + 擴展
    RETRYIX_KERNEL_STRATEGY_OPENCL11_BASIC, // OpenCL 1.1 基礎
    RETRYIX_KERNEL_STRATEGY_FALLBACK,      // 完全回退
    RETRYIX_KERNEL_STRATEGY_COUNT
} retryix_kernel_strategy_t;

// 內核優化等級
typedef enum {
    RETRYIX_KERNEL_OPT_NONE = 0,
    RETRYIX_KERNEL_OPT_BASIC = 1,
    RETRYIX_KERNEL_OPT_AGGRESSIVE = 2,
    RETRYIX_KERNEL_OPT_VENDOR_SPECIFIC = 3
} retryix_kernel_opt_level_t;

// 內核變體描述符
typedef struct {
    char name[64];                          // 內核名稱
    retryix_kernel_strategy_t strategy;     // 編譯策略
    char* source_code;                      // 源碼
    size_t source_length;                   // 源碼長度
    char build_options[512];                // 編譯選項
    cl_program program;                     // 編譯後程序
    cl_kernel kernel;                       // 內核對象
    bool is_compiled;                       // 是否已編譯
    double compile_time;                    // 編譯耗時
    uint64_t last_used;                     // 最後使用時間
    uint32_t use_count;                     // 使用次數
} retryix_kernel_variant_t;

// 內核模板定義
typedef struct {
    char template_name[64];                 // 模板名稱
    char* base_source;                      // 基礎源碼
    retryix_kernel_variant_t variants[RETRYIX_KERNEL_STRATEGY_COUNT]; // 所有變體
    int active_variant;                     // 當前活動變體
    bool is_universal;                      // 是否為通用模板
} retryix_kernel_template_t;

// 內核管理上下文
typedef struct {
    cl_context context;
    cl_device_id device;
    cl_command_queue queue;
    
    // 設備能力
    int opencl_major, opencl_minor;
    bool supports_fp64;
    bool supports_atomic_32;
    bool supports_atomic_64;
    bool supports_images;
    bool supports_svm;
    size_t max_work_group_size;
    size_t max_compute_units;
    
    // 內核模板池
    retryix_kernel_template_t* templates;
    size_t template_count;
    size_t template_capacity;
    
    // 編譯統計
    uint64_t total_compiles;
    uint64_t cache_hits;
    uint64_t cache_misses;
    double total_compile_time;
    
    // 運行統計
    uint64_t total_executions;
    double total_execution_time;
    size_t peak_memory_usage;
} retryix_kernel_context_t;

// 全局內核管理器
static retryix_kernel_context_t* g_kernel_context = NULL;

// === 內核源碼模板庫 ===

// 通用原子操作模板
static const char* UNIVERSAL_ATOMIC_TEMPLATE = 
"// RetryIX Universal Atomic Operations Template\n"
"#ifdef RETRYIX_OPENCL20\n"
"  #define RETRYIX_ATOMIC_ADD(ptr, val) atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)\n"
"  #define RETRYIX_ATOMIC_INC(ptr) atomic_fetch_add_explicit(ptr, 1, memory_order_relaxed)\n"
"  #define RETRYIX_ATOMIC_CAS(ptr, expected, desired) atomic_compare_exchange_weak_explicit(ptr, &expected, desired, memory_order_relaxed, memory_order_relaxed)\n"
"#elif defined(RETRYIX_OPENCL12_EXT)\n"
"  #pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable\n"
"  #pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable\n"
"  #define RETRYIX_ATOMIC_ADD(ptr, val) atomic_add(ptr, val)\n"
"  #define RETRYIX_ATOMIC_INC(ptr) atomic_inc(ptr)\n"
"  #define RETRYIX_ATOMIC_CAS(ptr, expected, desired) atomic_cmpxchg(ptr, expected, desired)\n"
"#else\n"
"  // Fallback: Use work-group level synchronization\n"
"  #define RETRYIX_ATOMIC_ADD(ptr, val) do { barrier(CLK_GLOBAL_MEM_FENCE); *(ptr) += (val); barrier(CLK_GLOBAL_MEM_FENCE); } while(0)\n"
"  #define RETRYIX_ATOMIC_INC(ptr) do { barrier(CLK_GLOBAL_MEM_FENCE); (*(ptr))++; barrier(CLK_GLOBAL_MEM_FENCE); } while(0)\n"
"  #define RETRYIX_ATOMIC_CAS(ptr, expected, desired) ({ barrier(CLK_GLOBAL_MEM_FENCE); int old = *(ptr); if (old == (expected)) *(ptr) = (desired); barrier(CLK_GLOBAL_MEM_FENCE); old; })\n"
"#endif\n\n";

// 通用記憶體操作模板
static const char* UNIVERSAL_MEMORY_TEMPLATE =
"// RetryIX Universal Memory Operations Template\n"
"#ifdef RETRYIX_SVM_SUPPORT\n"
"  #define RETRYIX_GLOBAL_PTR(type) __global type*\n"
"  #define RETRYIX_MEM_FENCE() mem_fence(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE)\n"
"#else\n"
"  #define RETRYIX_GLOBAL_PTR(type) __global type*\n"
"  #define RETRYIX_MEM_FENCE() barrier(CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE)\n"
"#endif\n\n";

// 通用向量操作模板
static const char* UNIVERSAL_VECTOR_TEMPLATE =
"// RetryIX Universal Vector Operations Template\n"
"#ifdef RETRYIX_NATIVE_DOUBLE\n"
"  #define RETRYIX_REAL double\n"
"  #define RETRYIX_REAL4 double4\n"
"#else\n"
"  #define RETRYIX_REAL float\n"
"  #define RETRYIX_REAL4 float4\n"
"#endif\n\n";

// === 內部函數 ===

// 檢測設備能力
static void probe_device_capabilities(retryix_kernel_context_t* ctx) {
    char version[256] = {0};
    char extensions[4096] = {0};
    
    clGetDeviceInfo(ctx->device, CL_DEVICE_VERSION, sizeof(version), version, NULL);
    clGetDeviceInfo(ctx->device, CL_DEVICE_EXTENSIONS, sizeof(extensions), extensions, NULL);
    
    // 解析 OpenCL 版本
    sscanf(version, "OpenCL %d.%d", &ctx->opencl_major, &ctx->opencl_minor);
    
    // 檢測擴展支援
    ctx->supports_fp64 = (strstr(extensions, "cl_khr_fp64") != NULL);
    ctx->supports_atomic_32 = (strstr(extensions, "cl_khr_global_int32_base_atomics") != NULL);
    ctx->supports_atomic_64 = (strstr(extensions, "cl_khr_int64_base_atomics") != NULL);
    ctx->supports_images = (strstr(extensions, "cl_khr_image2d_from_buffer") != NULL);
    
    // SVM 支援檢測
    if (ctx->opencl_major >= 2) {
        cl_bitfield svm_caps = 0;
        clGetDeviceInfo(ctx->device, CL_DEVICE_SVM_CAPABILITIES, sizeof(svm_caps), &svm_caps, NULL);
        ctx->supports_svm = (svm_caps != 0);
    }
    
    // 工作組能力
    clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(ctx->max_work_group_size), &ctx->max_work_group_size, NULL);
    clGetDeviceInfo(ctx->device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(ctx->max_compute_units), &ctx->max_compute_units, NULL);
    
    printf("RetryIX Kernel Context - Device Capabilities:\n");
    printf("  OpenCL Version: %d.%d\n", ctx->opencl_major, ctx->opencl_minor);
    printf("  FP64 Support: %s\n", ctx->supports_fp64 ? "YES" : "NO");
    printf("  32-bit Atomics: %s\n", ctx->supports_atomic_32 ? "YES" : "NO");
    printf("  64-bit Atomics: %s\n", ctx->supports_atomic_64 ? "YES" : "NO");
    printf("  SVM Support: %s\n", ctx->supports_svm ? "YES" : "NO");
    printf("  Max Work Group: %zu\n", ctx->max_work_group_size);
    printf("  Compute Units: %zu\n", ctx->max_compute_units);
}

// 選擇最佳內核策略
static retryix_kernel_strategy_t select_optimal_strategy(retryix_kernel_context_t* ctx) {
    if (ctx->opencl_major >= 2 && ctx->supports_atomic_32) {
        return RETRYIX_KERNEL_STRATEGY_OPENCL20;
    } else if (ctx->opencl_major >= 1 && ctx->opencl_minor >= 2 && ctx->supports_atomic_32) {
        return RETRYIX_KERNEL_STRATEGY_OPENCL12_EXT;
    } else if (ctx->opencl_major >= 1 && ctx->opencl_minor >= 1) {
        return RETRYIX_KERNEL_STRATEGY_OPENCL11_BASIC;
    } else {
        return RETRYIX_KERNEL_STRATEGY_FALLBACK;
    }
}

// 生成預處理器定義
static void generate_preprocessor_defines(retryix_kernel_context_t* ctx, retryix_kernel_strategy_t strategy, char* defines, size_t max_len) {
    snprintf(defines, max_len, "#define RETRYIX_DEVICE_COMPUTE_UNITS %zu\n", ctx->max_compute_units);
    
    switch (strategy) {
        case RETRYIX_KERNEL_STRATEGY_OPENCL20:
            strncat(defines, "#define RETRYIX_OPENCL20 1\n", max_len - strlen(defines) - 1);
            break;
        case RETRYIX_KERNEL_STRATEGY_OPENCL12_EXT:
            strncat(defines, "#define RETRYIX_OPENCL12_EXT 1\n", max_len - strlen(defines) - 1);
            break;
        case RETRYIX_KERNEL_STRATEGY_OPENCL11_BASIC:
            strncat(defines, "#define RETRYIX_OPENCL11_BASIC 1\n", max_len - strlen(defines) - 1);
            break;
        default:
            strncat(defines, "#define RETRYIX_FALLBACK 1\n", max_len - strlen(defines) - 1);
            break;
    }
    
    if (ctx->supports_fp64) {
        strncat(defines, "#define RETRYIX_NATIVE_DOUBLE 1\n", max_len - strlen(defines) - 1);
    }
    
    if (ctx->supports_svm) {
        strncat(defines, "#define RETRYIX_SVM_SUPPORT 1\n", max_len - strlen(defines) - 1);
    }
    
    if (ctx->supports_atomic_32) {
        strncat(defines, "#define RETRYIX_ATOMIC_32 1\n", max_len - strlen(defines) - 1);
    }
}

// 生成完整內核源碼
static char* generate_kernel_source(retryix_kernel_context_t* ctx, const char* user_source, retryix_kernel_strategy_t strategy) {
    char defines[1024] = {0};
    generate_preprocessor_defines(ctx, strategy, defines, sizeof(defines));
    
    // 計算所需緩衝區大小
    size_t total_size = strlen(defines) + strlen(UNIVERSAL_ATOMIC_TEMPLATE) + 
                       strlen(UNIVERSAL_MEMORY_TEMPLATE) + strlen(UNIVERSAL_VECTOR_TEMPLATE) + 
                       strlen(user_source) + 512; // 額外空間
    
    char* complete_source = (char*)malloc(total_size);
    if (!complete_source) return NULL;
    
    // 組裝完整源碼
    snprintf(complete_source, total_size,
        "%s\n%s%s%s\n// === User Kernel Code ===\n%s\n",
        defines, UNIVERSAL_ATOMIC_TEMPLATE, UNIVERSAL_MEMORY_TEMPLATE, 
        UNIVERSAL_VECTOR_TEMPLATE, user_source);
    
    return complete_source;
}

// 編譯內核變體
static int compile_kernel_variant(retryix_kernel_context_t* ctx, retryix_kernel_variant_t* variant, const char* user_source) {
    if (variant->is_compiled) return 0; // 已編譯
    
    clock_t start = clock();
    
    // 生成完整源碼
    char* complete_source = generate_kernel_source(ctx, user_source, variant->strategy);
    if (!complete_source) return -1;
    
    variant->source_code = complete_source;
    variant->source_length = strlen(complete_source);
    
    // 創建程序
    cl_int err;
    variant->program = clCreateProgramWithSource(ctx->context, 1, (const char**)&variant->source_code, 
                                               &variant->source_length, &err);
    if (err != CL_SUCCESS) {
        printf("Failed to create program for variant %s: %d\n", variant->name, err);
        free(complete_source);
        return -1;
    }
    
    // 設定編譯選項
    const char* strategy_names[] = {
        "-cl-std=CL2.0 -cl-fast-relaxed-math",
        "-cl-std=CL1.2 -cl-fast-relaxed-math", 
        "-cl-std=CL1.1 -cl-unsafe-math-optimizations",
        "-cl-std=CL1.0"
    };
    
    snprintf(variant->build_options, sizeof(variant->build_options), "%s -DRETRYIX_VARIANT_%d=1", 
             strategy_names[variant->strategy], (int)variant->strategy);
    
    // 編譯程序
    err = clBuildProgram(variant->program, 1, &ctx->device, variant->build_options, NULL, NULL);
    
    if (err != CL_SUCCESS) {
        // 獲取編譯日誌
        size_t log_size;
        clGetProgramBuildInfo(variant->program, ctx->device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        if (log_size > 1) {
            char* build_log = (char*)malloc(log_size);
            clGetProgramBuildInfo(variant->program, ctx->device, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
            printf("Kernel compilation failed for %s:\n%s\n", variant->name, build_log);
            free(build_log);
        }
        
        clReleaseProgram(variant->program);
        variant->program = NULL;
        return -1;
    }
    
    // 創建內核對象
    variant->kernel = clCreateKernel(variant->program, variant->name, &err);
    if (err != CL_SUCCESS) {
        printf("Failed to create kernel %s: %d\n", variant->name, err);
        clReleaseProgram(variant->program);
        variant->program = NULL;
        return -1;
    }
    
    variant->is_compiled = true;
    variant->compile_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    // 更新統計
    ctx->total_compiles++;
    ctx->total_compile_time += variant->compile_time;
    
    printf("Kernel variant compiled: %s (%.3f seconds, strategy %d)\n", 
           variant->name, variant->compile_time, variant->strategy);
    
    return 0;
}

// === 公開 API ===

// 初始化內核管理器
int retryix_kernel_init(cl_context context, cl_device_id device, cl_command_queue queue) {
    if (g_kernel_context) return 0; // 已初始化
    
    retryix_kernel_context_t* ctx = (retryix_kernel_context_t*)calloc(1, sizeof(retryix_kernel_context_t));
    if (!ctx) return -1;
    
    ctx->context = context;
    ctx->device = device;
    ctx->queue = queue;
    
    // 檢測設備能力
    probe_device_capabilities(ctx);
    
    // 初始化模板池
    ctx->template_capacity = 32;
    ctx->templates = (retryix_kernel_template_t*)calloc(ctx->template_capacity, sizeof(retryix_kernel_template_t));
    if (!ctx->templates) {
        free(ctx);
        return -1;
    }
    
    g_kernel_context = ctx;
    
    printf("RetryIX Kernel Manager initialized\n");
    return 0;
}

// 註冊內核模板
int retryix_kernel_register_template(const char* template_name, const char* kernel_name, const char* source_code) {
    if (!g_kernel_context || !template_name || !kernel_name || !source_code) return -1;
    
    retryix_kernel_context_t* ctx = g_kernel_context;
    
    // 檢查容量
    if (ctx->template_count >= ctx->template_capacity) {
        ctx->template_capacity *= 2;
        ctx->templates = (retryix_kernel_template_t*)realloc(ctx->templates, 
                                                           ctx->template_capacity * sizeof(retryix_kernel_template_t));
        if (!ctx->templates) return -1;
    }
    
    retryix_kernel_template_t* tmpl = &ctx->templates[ctx->template_count];
    strncpy(tmpl->template_name, template_name, sizeof(tmpl->template_name) - 1);
    
    tmpl->base_source = strdup(source_code);
    tmpl->active_variant = -1;
    tmpl->is_universal = true;
    
    // 為所有策略創建變體
    for (int i = 0; i < RETRYIX_KERNEL_STRATEGY_COUNT; i++) {
        retryix_kernel_variant_t* variant = &tmpl->variants[i];
        strncpy(variant->name, kernel_name, sizeof(variant->name) - 1);
        variant->strategy = (retryix_kernel_strategy_t)i;
        variant->is_compiled = false;
        variant->program = NULL;
        variant->kernel = NULL;
        variant->source_code = NULL;
    }
    
    ctx->template_count++;
    
    printf("Kernel template registered: %s (%s)\n", template_name, kernel_name);
    return 0;
}

// 編譯最佳內核變體
cl_kernel retryix_kernel_compile_best(const char* template_name) {
    if (!g_kernel_context || !template_name) return NULL;
    
    retryix_kernel_context_t* ctx = g_kernel_context;
    
    // 查找模板
    retryix_kernel_template_t* tmpl = NULL;
    for (size_t i = 0; i < ctx->template_count; i++) {
        if (strcmp(ctx->templates[i].template_name, template_name) == 0) {
            tmpl = &ctx->templates[i];
            break;
        }
    }
    
    if (!tmpl) {
        printf("Template not found: %s\n", template_name);
        return NULL;
    }
    
    // 如果已有活動變體且已編譯，返回快取結果
    if (tmpl->active_variant >= 0 && tmpl->variants[tmpl->active_variant].is_compiled) {
        ctx->cache_hits++;
        tmpl->variants[tmpl->active_variant].use_count++;
        tmpl->variants[tmpl->active_variant].last_used = (uint64_t)time(NULL);
        return tmpl->variants[tmpl->active_variant].kernel;
    }
    
    ctx->cache_misses++;
    
    // 選擇最佳策略
    retryix_kernel_strategy_t best_strategy = select_optimal_strategy(ctx);
    
    // 按優先級嘗試編譯
    retryix_kernel_strategy_t try_order[] = {best_strategy, RETRYIX_KERNEL_STRATEGY_OPENCL12_EXT, 
                                           RETRYIX_KERNEL_STRATEGY_OPENCL11_BASIC, RETRYIX_KERNEL_STRATEGY_FALLBACK};
    
    for (int i = 0; i < 4; i++) {
        retryix_kernel_strategy_t strategy = try_order[i];
        retryix_kernel_variant_t* variant = &tmpl->variants[strategy];
        
        if (compile_kernel_variant(ctx, variant, tmpl->base_source) == 0) {
            tmpl->active_variant = strategy;
            variant->use_count++;
            variant->last_used = (uint64_t)time(NULL);
            
            printf("Successfully compiled kernel: %s with strategy %d\n", template_name, strategy);
            return variant->kernel;
        }
        
        printf("Strategy %d failed for %s, trying next...\n", strategy, template_name);
    }
    
    printf("All compilation strategies failed for template: %s\n", template_name);
    return NULL;
}

// 執行內核
int retryix_kernel_execute(const char* template_name, size_t global_work_size, size_t local_work_size, ...) {
    if (!g_kernel_context || !template_name) return -1;
    
    retryix_kernel_context_t* ctx = g_kernel_context;
    cl_kernel kernel = retryix_kernel_compile_best(template_name);
    if (!kernel) return -1;
    
    // 處理可變參數（內核參數）
    va_list args;
    va_start(args, local_work_size);
    
    int arg_index = 0;
    while (1) {
        void* arg_value = va_arg(args, void*);
        if (!arg_value) break; // NULL 表示參數結束
        
        size_t arg_size = va_arg(args, size_t);
        cl_int err = clSetKernelArg(kernel, arg_index++, arg_size, arg_value);
        if (err != CL_SUCCESS) {
            va_end(args);
            printf("Failed to set kernel argument %d: %d\n", arg_index - 1, err);
            return -1;
        }
    }
    
    va_end(args);
    
    // 執行內核
    clock_t start = clock();
    size_t local = (local_work_size > 0) ? local_work_size : 0;
    cl_int err = clEnqueueNDRangeKernel(ctx->queue, kernel, 1, NULL, &global_work_size, 
                                       local > 0 ? &local : NULL, 0, NULL, NULL);
    
    if (err != CL_SUCCESS) {
        printf("Kernel execution failed: %d\n", err);
        return -1;
    }
    
    clFinish(ctx->queue);
    
    double execution_time = (double)(clock() - start) / CLOCKS_PER_SEC;
    
    // 更新統計
    ctx->total_executions++;
    ctx->total_execution_time += execution_time;
    
    printf("Kernel executed: %s (%.3f ms, global=%zu, local=%zu)\n", 
           template_name, execution_time * 1000.0, global_work_size, local_work_size);
    
    return 0;
}

// 內核性能統計
void retryix_kernel_print_stats(void) {
    if (!g_kernel_context) {
        printf("Kernel manager not initialized\n");
        return;
    }
    
    retryix_kernel_context_t* ctx = g_kernel_context;
    
    printf("\n=== RetryIX Kernel Statistics ===\n");
    printf("Total Templates: %zu\n", ctx->template_count);
    printf("Total Compiles: %llu\n", (unsigned long long)ctx->total_compiles);
    printf("Cache Hits: %llu\n", (unsigned long long)ctx->cache_hits);
    printf("Cache Misses: %llu\n", (unsigned long long)ctx->cache_misses);
    printf("Cache Hit Rate: %.1f%%\n", ctx->cache_hits + ctx->cache_misses > 0 ? 
           (double)ctx->cache_hits / (ctx->cache_hits + ctx->cache_misses) * 100.0 : 0.0);
    printf("Total Compile Time: %.3f seconds\n", ctx->total_compile_time);
    printf("Average Compile Time: %.3f seconds\n", ctx->total_compiles > 0 ? 
           ctx->total_compile_time / ctx->total_compiles : 0.0);
    printf("Total Executions: %llu\n", (unsigned long long)ctx->total_executions);
    printf("Total Execution Time: %.3f seconds\n", ctx->total_execution_time);
    printf("Average Execution Time: %.3f ms\n", ctx->total_executions > 0 ? 
           (ctx->total_execution_time / ctx->total_executions) * 1000.0 : 0.0);
    
    printf("\nActive Templates:\n");
    for (size_t i = 0; i < ctx->template_count; i++) {
        retryix_kernel_template_t* tmpl = &ctx->templates[i];
        if (tmpl->active_variant >= 0) {
            retryix_kernel_variant_t* variant = &tmpl->variants[tmpl->active_variant];
            printf("  %s: Strategy %d, Used %u times, Compile time %.3fs\n",
                   tmpl->template_name, tmpl->active_variant, variant->use_count, variant->compile_time);
        } else {
            printf("  %s: Not compiled\n", tmpl->template_name);
        }
    }
    printf("==================================\n\n");
}

// 清理內核管理器
void retryix_kernel_cleanup(void) {
    if (!g_kernel_context) return;
    
    retryix_kernel_context_t* ctx = g_kernel_context;
    
    printf("RetryIX Kernel Manager Cleanup\n");
    
    // 釋放所有內核資源
    for (size_t i = 0; i < ctx->template_count; i++) {
        retryix_kernel_template_t* tmpl = &ctx->templates[i];
        
        if (tmpl->base_source) {
            free(tmpl->base_source);
        }
        
        for (int j = 0; j < RETRYIX_KERNEL_STRATEGY_COUNT; j++) {
            retryix_kernel_variant_t* variant = &tmpl->variants[j];
            if (variant->kernel) {
                clReleaseKernel(variant->kernel);
            }
            if (variant->program) {
                clReleaseProgram(variant->program);
            }
            if (variant->source_code) {
                free(variant->source_code);
            }
        }
    }
    
    // 打印最終統計
    retryix_kernel_print_stats();
    
    free(ctx->templates);
    free(ctx);
    g_kernel_context = NULL;
    
    printf("Kernel manager cleanup complete\n");
}

// 強化版 OpenCL 原子加法 demo，可被 Python 呼叫
#ifdef _WIN32
#define RETRYIX_API __declspec(dllexport)
#else
#define RETRYIX_API
#endif

// Demo: 多裝置、多平台自動偵測，支援 SVM/Buffer，驗證原子加法正確性
RETRYIX_API void retryix_kernel_atomic_add_demo(void) {
    printf("[RetryIX] Atomic Add Demo Start\n");
    cl_int err;
    cl_uint num_platforms = 0;
    cl_platform_id platforms[8] = {0};
    clGetPlatformIDs(8, platforms, &num_platforms);
    if (num_platforms == 0) { printf("No OpenCL platform found!\n"); return; }

    for (cl_uint p = 0; p < num_platforms; ++p) {
        cl_platform_id platform = platforms[p];
        cl_uint num_devices = 0;
        cl_device_id devices[8] = {0};
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_ALL, 8, devices, &num_devices);
        if (num_devices == 0) continue;
        for (cl_uint d = 0; d < num_devices; ++d) {
            cl_device_id device = devices[d];
            char devname[256] = {0};
            clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(devname), devname, NULL);
            printf("\n[Device] %s\n", devname);

            // 查詢 SVM 支援
            cl_bitfield svm_caps = 0;
            clGetDeviceInfo(device, CL_DEVICE_SVM_CAPABILITIES, sizeof(svm_caps), &svm_caps, NULL);
            int use_svm = (svm_caps & (CL_DEVICE_SVM_COARSE_GRAIN_BUFFER|CL_DEVICE_SVM_FINE_GRAIN_BUFFER)) ? 1 : 0;

            cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
            cl_int err = CL_SUCCESS;
            cl_command_queue queue = rixCreateQueue(context, device, &err);
            if (err != CL_SUCCESS || !queue) {
                fprintf(stderr, "Create queue failed: %s\n", rixCLErrorName(err));
                clReleaseContext(context);
                continue;
            }


            // Kernel 原始碼
            const char* kernel_source =
                "__kernel void atomic_add_demo(__global int* buf, int N) {\n"
                "  int gid = get_global_id(0);\n"
                "  if (gid < N) atomic_add(&buf[0], 1);\n"
                "}\n";

            cl_program prog = rixBuildProgram(context, device, kernel_source, "-cl-std=CL2.0");
            if (!prog) {
                clReleaseCommandQueue(queue);
                clReleaseContext(context);
                continue;
            }
            cl_kernel kernel = clCreateKernel(prog, "atomic_add_demo", &err);

            const int N = 1024 * 1024;
            int* hostbuf = NULL;
            cl_mem buf = NULL;
            if (use_svm) {
                hostbuf = (int*)clSVMAlloc(context, CL_MEM_READ_WRITE, sizeof(int), 0);
                *hostbuf = 0;
            } else {
                hostbuf = (int*)malloc(sizeof(int));
                *hostbuf = 0;
                buf = clCreateBuffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, sizeof(int), hostbuf, &err);
            }

            // 設定 kernel 參數
            if (use_svm) {
                clSetKernelArgSVMPointer(kernel, 0, hostbuf);
            } else {
                clSetKernelArg(kernel, 0, sizeof(cl_mem), &buf);
            }
            clSetKernelArg(kernel, 1, sizeof(int), &N);

            size_t gws = N;
            size_t lws = 0;
            if (use_svm) {
                clEnqueueSVMUnmap(queue, hostbuf, 0, NULL, NULL); // 保證 hostbuf 可用
            }
            clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &gws, lws ? &lws : NULL, 0, NULL, NULL);
            clFinish(queue);

            // 讀回結果
            int result = 0;
            if (use_svm) {
                result = *hostbuf;
                clSVMFree(context, hostbuf);
            } else {
                clEnqueueReadBuffer(queue, buf, CL_TRUE, 0, sizeof(int), &result, 0, NULL, NULL);
                clReleaseMemObject(buf);
                free(hostbuf);
            }

            printf("[Result] Atomic add %d times, final value = %d\n", N, result);
            if (result == N) {
                printf("[PASS] Atomic add correct!\n");
            } else {
                printf("[FAIL] Atomic add incorrect!\n");
            }

            clReleaseKernel(kernel);
            clReleaseProgram(prog);
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
        }
    }
    printf("[RetryIX] Atomic Add Demo End\n");
}
// END DEMO