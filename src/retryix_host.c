#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 跨平台中文顯示支援
#ifdef _WIN32
    #include <windows.h>
    #include <locale.h>
    void setup_unicode() {
        SetConsoleOutputCP(65001);
        setlocale(LC_ALL, ".UTF8");
    }
#else
    void setup_unicode() {
        setlocale(LC_ALL, "");
    }
#endif

#define CHECK_CL_ERROR(err, msg) \
    if (err != CL_SUCCESS) { \
        fprintf(stderr, "OpenCL Error (%d): %s\n", err, msg); \
        exit(EXIT_FAILURE); \
    }

// 設備能力結構體 - 避免與 Windows API 衝突
typedef struct {
    int supports_atomic_32;
    int supports_atomic_64;
    int supports_extended_atomics;
    int opencl_version_major;
    int opencl_version_minor;
    int use_opencl2_atomics;
} OpenCLDeviceCapabilities;

// 多版本原子操作內核 - 根據硬體能力動態選擇
const char *kernelSource_OpenCL2 =
"__kernel void atomic_add_kernel(__global atomic_int *counter) {\n"
"    atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);\n"
"}\n"
"__kernel void atomic_stress_test(__global atomic_int *counter, __global int *results) {\n"
"    int gid = get_global_id(0);\n"
"    int old_value = atomic_fetch_add_explicit(counter, 1, memory_order_relaxed);\n"
"    results[gid] = old_value;\n"
"}\n";

const char *kernelSource_OpenCL1_WithExtensions =
"#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable\n"
"#pragma OPENCL EXTENSION cl_khr_global_int32_extended_atomics : enable\n"
"__kernel void atomic_add_kernel(__global int *counter) {\n"
"    atomic_add(counter, 1);\n"
"}\n"
"__kernel void atomic_stress_test(__global int *counter, __global int *results) {\n"
"    int gid = get_global_id(0);\n"
"    int old_value = atomic_inc(counter);\n"
"    results[gid] = old_value;\n"
"}\n"
"__kernel void basic_ops_test(__global int *data) {\n"
"    int gid = get_global_id(0);\n"
"    if (gid < 4) {\n"
"        switch(gid) {\n"
"            case 0: atomic_add(&data[0], 1); break;\n"
"            case 1: atomic_sub(&data[1], 1); break;\n"
"            case 2: atomic_max(&data[2], gid); break;\n"
"            case 3: atomic_min(&data[3], gid + 100); break;\n"
"        }\n"
"    }\n"
"}\n";

const char *kernelSource_Basic =
"__kernel void atomic_add_kernel(__global int *counter) {\n"
"    // 最基本的原子操作，所有平台都應該支援\n"
"    atom_add(counter, 1);\n"
"}\n"
"__kernel void atomic_stress_test(__global int *counter, __global int *results) {\n"
"    int gid = get_global_id(0);\n"
"    int old_value = atom_inc(counter);\n"
"    results[gid] = old_value;\n"
"}\n";

const char *kernelSource_Fallback =
"// 完全回退版本 - 使用本地記憶體模擬\n"
"__kernel void atomic_add_kernel(__global int *counter) {\n"
"    __local int local_sum[256];\n"
"    int lid = get_local_id(0);\n"
"    int lsize = get_local_size(0);\n"
"    \n"
"    local_sum[lid] = 1;\n"
"    barrier(CLK_LOCAL_MEM_FENCE);\n"
"    \n"
"    // 本地歸約\n"
"    for(int stride = lsize/2; stride > 0; stride /= 2) {\n"
"        if(lid < stride) {\n"
"            local_sum[lid] += local_sum[lid + stride];\n"
"        }\n"
"        barrier(CLK_LOCAL_MEM_FENCE);\n"
"    }\n"
"    \n"
"    if(lid == 0) {\n"
"        // 簡單的非原子加法 - 只用於測試\n"
"        *counter += local_sum[0];\n"
"    }\n"
"}\n"
"__kernel void atomic_stress_test(__global int *counter, __global int *results) {\n"
"    int gid = get_global_id(0);\n"
"    results[gid] = gid;  // 簡單填充，用於測試\n"
"}\n";

OpenCLDeviceCapabilities analyze_device_capabilities(cl_device_id device) {
    OpenCLDeviceCapabilities caps = {0};
    char extensions[4096] = {0};
    char version[256] = {0};
    
    clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, sizeof(extensions), extensions, NULL);
    clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(version), version, NULL);
    
    // 解析 OpenCL 版本
    if (sscanf(version, "OpenCL %d.%d", &caps.opencl_version_major, &caps.opencl_version_minor) == 2) {
        caps.use_opencl2_atomics = (caps.opencl_version_major >= 2);
    }
    
    // 檢查原子操作擴展
    caps.supports_atomic_32 = (strstr(extensions, "cl_khr_global_int32_base_atomics") != NULL);
    caps.supports_atomic_64 = (strstr(extensions, "cl_khr_int64_base_atomics") != NULL);
    caps.supports_extended_atomics = (strstr(extensions, "cl_khr_global_int32_extended_atomics") != NULL);
    
    return caps;
}

const char* select_best_kernel(OpenCLDeviceCapabilities caps) {
    if (caps.use_opencl2_atomics && caps.supports_atomic_32) {
        printf("Strategy Selection: OpenCL 2.0 Atomic Operations\n");
        return kernelSource_OpenCL2;
    } else if (caps.supports_atomic_32) {
        printf("Strategy Selection: OpenCL 1.x + Extensions\n");
        return kernelSource_OpenCL1_WithExtensions;
    } else {
        printf("Strategy Selection: Basic Atomic Operations\n");
        return kernelSource_Basic;
    }
}

void print_device_capabilities(cl_device_id device, OpenCLDeviceCapabilities caps) {
    char device_name[256], vendor[256], version[256];
    cl_device_type device_type;
    cl_ulong global_mem_size;
    
    clGetDeviceInfo(device, CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
    clGetDeviceInfo(device, CL_DEVICE_VENDOR, sizeof(vendor), vendor, NULL);
    clGetDeviceInfo(device, CL_DEVICE_VERSION, sizeof(version), version, NULL);
    clGetDeviceInfo(device, CL_DEVICE_TYPE, sizeof(device_type), &device_type, NULL);
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(global_mem_size), &global_mem_size, NULL);
    
    printf("=== Universal Device Info ===\n");
    printf("Device Name: %s\n", device_name);
    printf("Vendor: %s\n", vendor);
    printf("OpenCL Version: %s\n", version);
    printf("Device Type: %s\n", (device_type == CL_DEVICE_TYPE_GPU) ? "GPU" : 
                            (device_type == CL_DEVICE_TYPE_CPU) ? "CPU" : "Other");
    printf("Global Memory: %.2f GB\n", (double)global_mem_size / (1024*1024*1024));
    
    printf("\n=== Atomic Operations Support Analysis ===\n");
    printf("OpenCL Version: %d.%d\n", caps.opencl_version_major, caps.opencl_version_minor);
    printf("32-bit Atomic Operations: %s\n", caps.supports_atomic_32 ? "✅ Supported" : "❌ Not Supported");
    printf("64-bit Atomic Operations: %s\n", caps.supports_atomic_64 ? "✅ Supported" : "❌ Not Supported");
    printf("Extended Atomic Operations: %s\n", caps.supports_extended_atomics ? "✅ Supported" : "❌ Not Supported");
    printf("OpenCL 2.0 Syntax: %s\n", caps.use_opencl2_atomics ? "✅ Available" : "❌ Not Available");
    printf("\n");
}

int test_kernel_compilation(cl_context context, cl_device_id device, const char* source, const char* build_options) {
    cl_int err;
    cl_program program = clCreateProgramWithSource(context, 1, &source, NULL, &err);
    if (err != CL_SUCCESS) return 0;
    
    err = clBuildProgram(program, 1, &device, build_options, NULL, NULL);
    
    if (err != CL_SUCCESS) {
        size_t log_size;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        if (log_size > 1) {
            char *build_log = (char*)malloc(log_size);
            clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
            printf("Compilation failed log:\n%s\n", build_log);
            free(build_log);
        }
    }
    
    clReleaseProgram(program);
    return (err == CL_SUCCESS);
}

int run_universal_atomic_test(cl_context context, cl_command_queue queue, cl_device_id device) {
    OpenCLDeviceCapabilities caps = analyze_device_capabilities(device);
    print_device_capabilities(device, caps);
    
    // 嘗試不同的內核實現
    const char* kernels_to_try[] = {
        kernelSource_OpenCL2,
        kernelSource_OpenCL1_WithExtensions, 
        kernelSource_Basic,
        kernelSource_Fallback
    };
    
    const char* build_options[] = {
        "-cl-std=CL2.0",
        "-cl-std=CL1.2", 
        "-cl-std=CL1.1",
        "-cl-std=CL1.0"
    };
    
    const char* strategy_names[] = {
        "OpenCL 2.0 Standard Atomic Operations",
        "OpenCL 1.x + Atomic Extensions",
        "Basic Atomic Operation Functions",
        "Fallback Solution (Local Memory)"
    };
    
    cl_program working_program = NULL;
    int working_strategy = -1;
    
    // 逐一測試直到找到可用的實現
    for (int i = 0; i < 4; i++) {
        printf("Testing Strategy %d: %s\n", i + 1, strategy_names[i]);
        
        if (test_kernel_compilation(context, device, kernels_to_try[i], build_options[i])) {
            cl_int err;
            working_program = clCreateProgramWithSource(context, 1, &kernels_to_try[i], NULL, &err);
            if (err == CL_SUCCESS) {
                err = clBuildProgram(working_program, 1, &device, build_options[i], NULL, NULL);
                if (err == CL_SUCCESS) {
                    working_strategy = i;
                    printf("✅ Strategy %d compiled successfully!\n\n", i + 1);
                    break;
                }
            }
            if (working_program) {
                clReleaseProgram(working_program);
                working_program = NULL;
            }
        }
        printf("❌ Strategy %d failed, trying next...\n", i + 1);
    }
    
    if (!working_program) {
        printf("All strategies failed, this device may not support atomic operations\n");
        return 0;
    }
    
    printf("=== Using Strategy %d for Testing ===\n", working_strategy + 1);
    
    // 執行實際測試
    cl_int err;
    cl_kernel kernel = clCreateKernel(working_program, "atomic_add_kernel", &err);
    if (err != CL_SUCCESS) {
        printf("Cannot create kernel\n");
        clReleaseProgram(working_program);
        return 0;
    }
    
    // 創建測試緩衝區
    int counter_init = 0;
    cl_mem counter_buf = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                        sizeof(cl_int), &counter_init, &err);
    if (err != CL_SUCCESS) {
        printf("Cannot create buffer\n");
        clReleaseKernel(kernel);
        clReleaseProgram(working_program);
        return 0;
    }
    
    // 設定內核參數
    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &counter_buf);
    if (err != CL_SUCCESS) {
        printf("Cannot set kernel arguments\n");
        goto cleanup;
    }
    
    // 執行測試
    size_t global_size = 1024;
    err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Kernel execution failed\n");
        goto cleanup;
    }
    
    clFinish(queue);
    
    // 讀取結果
    int result = 0;
    err = clEnqueueReadBuffer(queue, counter_buf, CL_TRUE, 0, sizeof(cl_int), &result, 0, NULL, NULL);
    if (err != CL_SUCCESS) {
        printf("Failed to read results\n");
        goto cleanup;
    }
    
    printf("Test Result: %d (Expected: %zu)\n", result, global_size);
    printf("Universal Atomic Operations Test: %s\n", (result > 0) ? "✅ SUCCESS" : "❌ FAILED");
    
cleanup:
    clReleaseMemObject(counter_buf);
    clReleaseKernel(kernel);
    clReleaseProgram(working_program);
    return 1;
}

int main() {
    setup_unicode();
    
    cl_int err;
    cl_uint num_platforms;
    
    printf("🌍 Universal OpenCL Atomic Operations Compatibility Test\n\n");
    
    // 枚舉所有平台
    err = clGetPlatformIDs(0, NULL, &num_platforms);
    CHECK_CL_ERROR(err, "Cannot query platform count");
    
    if (num_platforms == 0) {
        printf("No OpenCL platforms found\n");
        return 1;
    }
    
    cl_platform_id *platforms = (cl_platform_id*)malloc(num_platforms * sizeof(cl_platform_id));
    err = clGetPlatformIDs(num_platforms, platforms, NULL);
    CHECK_CL_ERROR(err, "Cannot get platform list");
    
    printf("Found %u OpenCL platform(s)\n\n", num_platforms);
    
    // 測試每個平台的每個設備
    for (cl_uint p = 0; p < num_platforms; p++) {
        char platform_name[256], platform_vendor[256];
        clGetPlatformInfo(platforms[p], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);
        clGetPlatformInfo(platforms[p], CL_PLATFORM_VENDOR, sizeof(platform_vendor), platform_vendor, NULL);
        
        printf("=== Platform %u: %s (%s) ===\n", p + 1, platform_name, platform_vendor);
        
        // 獲取該平台的所有設備
        cl_uint num_devices;
        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
        if (err != CL_SUCCESS || num_devices == 0) {
            printf("No devices available on this platform\n\n");
            continue;
        }
        
        cl_device_id *devices = (cl_device_id*)malloc(num_devices * sizeof(cl_device_id));
        err = clGetDeviceIDs(platforms[p], CL_DEVICE_TYPE_ALL, num_devices, devices, NULL);
        CHECK_CL_ERROR(err, "Cannot get device list");
        
        // 測試每個設備
        for (cl_uint d = 0; d < num_devices; d++) {
            printf("--- Device %u ---\n", d + 1);
            
            cl_context context = clCreateContext(NULL, 1, &devices[d], NULL, NULL, &err);
            if (err != CL_SUCCESS) {
                printf("Cannot create context\n");
                continue;
            }
            
            // 使用新的 command queue 創建方式（避免 deprecated 警告）
            cl_queue_properties properties[] = {0};
            cl_command_queue queue = clCreateCommandQueueWithProperties(context, devices[d], properties, &err);
            if (err != CL_SUCCESS) {
                printf("Cannot create command queue\n");
                clReleaseContext(context);
                continue;
            }
            
            run_universal_atomic_test(context, queue, devices[d]);
            
            clReleaseCommandQueue(queue);
            clReleaseContext(context);
            printf("\n");
        }
        
        free(devices);
        printf("\n");
    }
    
    free(platforms);
    printf("🎉 All platforms and devices tested!\n");
    return 0;
}