// retryix_cl_compat.h
// Unified OpenCL 1.2 / 2.0+ compatibility helpers for RetryIX
#ifndef RETRYIX_CL_COMPAT_H
#define RETRYIX_CL_COMPAT_H

// ── Version targets ──────────────────────────────────────────────────────────
// 預設鎖在 2.0；如需覆蓋可在編譯旗標指定 -DCL_TARGET_OPENCL_VERSION=120 / 200 / 300
#ifndef CL_TARGET_OPENCL_VERSION
  #define CL_TARGET_OPENCL_VERSION 200
#endif

// 若要在 2.0/3.0 SDK 仍可使用 1.2 API（如 clCreateCommandQueue），開啟下列開關
#ifndef CL_USE_DEPRECATED_OPENCL_1_2_APIS
  #define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#endif

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── Error name helper ────────────────────────────────────────────────────────
static inline const char* rixCLErrorName(cl_int err) {
    switch (err) {
        case CL_SUCCESS: return "CL_SUCCESS";
        case CL_DEVICE_NOT_FOUND: return "CL_DEVICE_NOT_FOUND";
        case CL_DEVICE_NOT_AVAILABLE: return "CL_DEVICE_NOT_AVAILABLE";
        case CL_COMPILER_NOT_AVAILABLE: return "CL_COMPILER_NOT_AVAILABLE";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
        case CL_OUT_OF_RESOURCES: return "CL_OUT_OF_RESOURCES";
        case CL_OUT_OF_HOST_MEMORY: return "CL_OUT_OF_HOST_MEMORY";
        case CL_PROFILING_INFO_NOT_AVAILABLE: return "CL_PROFILING_INFO_NOT_AVAILABLE";
        case CL_MEM_COPY_OVERLAP: return "CL_MEM_COPY_OVERLAP";
        case CL_IMAGE_FORMAT_MISMATCH: return "CL_IMAGE_FORMAT_MISMATCH";
        case CL_IMAGE_FORMAT_NOT_SUPPORTED: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
        case CL_BUILD_PROGRAM_FAILURE: return "CL_BUILD_PROGRAM_FAILURE";
        case CL_MAP_FAILURE: return "CL_MAP_FAILURE";
#ifdef CL_VERSION_1_2
        case CL_COMPILE_PROGRAM_FAILURE: return "CL_COMPILE_PROGRAM_FAILURE";
        case CL_LINKER_NOT_AVAILABLE: return "CL_LINKER_NOT_AVAILABLE";
        case CL_LINK_PROGRAM_FAILURE: return "CL_LINK_PROGRAM_FAILURE";
        case CL_DEVICE_PARTITION_FAILED: return "CL_DEVICE_PARTITION_FAILED";
        case CL_KERNEL_ARG_INFO_NOT_AVAILABLE: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
#endif
        case CL_INVALID_VALUE: return "CL_INVALID_VALUE";
        case CL_INVALID_DEVICE_TYPE: return "CL_INVALID_DEVICE_TYPE";
        case CL_INVALID_PLATFORM: return "CL_INVALID_PLATFORM";
        case CL_INVALID_DEVICE: return "CL_INVALID_DEVICE";
        case CL_INVALID_CONTEXT: return "CL_INVALID_CONTEXT";
        case CL_INVALID_QUEUE_PROPERTIES: return "CL_INVALID_QUEUE_PROPERTIES";
        case CL_INVALID_COMMAND_QUEUE: return "CL_INVALID_COMMAND_QUEUE";
        case CL_INVALID_HOST_PTR: return "CL_INVALID_HOST_PTR";
        case CL_INVALID_MEM_OBJECT: return "CL_INVALID_MEM_OBJECT";
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
        case CL_INVALID_IMAGE_SIZE: return "CL_INVALID_IMAGE_SIZE";
        case CL_INVALID_SAMPLER: return "CL_INVALID_SAMPLER";
        case CL_INVALID_BINARY: return "CL_INVALID_BINARY";
        case CL_INVALID_BUILD_OPTIONS: return "CL_INVALID_BUILD_OPTIONS";
        case CL_INVALID_PROGRAM: return "CL_INVALID_PROGRAM";
        case CL_INVALID_PROGRAM_EXECUTABLE: return "CL_INVALID_PROGRAM_EXECUTABLE";
        case CL_INVALID_KERNEL_NAME: return "CL_INVALID_KERNEL_NAME";
        case CL_INVALID_KERNEL_DEFINITION: return "CL_INVALID_KERNEL_DEFINITION";
        case CL_INVALID_KERNEL: return "CL_INVALID_KERNEL";
        case CL_INVALID_ARG_INDEX: return "CL_INVALID_ARG_INDEX";
        case CL_INVALID_ARG_VALUE: return "CL_INVALID_ARG_VALUE";
        case CL_INVALID_ARG_SIZE: return "CL_INVALID_ARG_SIZE";
        case CL_INVALID_KERNEL_ARGS: return "CL_INVALID_KERNEL_ARGS";
        case CL_INVALID_WORK_DIMENSION: return "CL_INVALID_WORK_DIMENSION";
        case CL_INVALID_WORK_GROUP_SIZE: return "CL_INVALID_WORK_GROUP_SIZE";
        case CL_INVALID_WORK_ITEM_SIZE: return "CL_INVALID_WORK_ITEM_SIZE";
        case CL_INVALID_GLOBAL_OFFSET: return "CL_INVALID_GLOBAL_OFFSET";
        case CL_INVALID_EVENT_WAIT_LIST: return "CL_INVALID_EVENT_WAIT_LIST";
        case CL_INVALID_EVENT: return "CL_INVALID_EVENT";
        case CL_INVALID_OPERATION: return "CL_INVALID_OPERATION";
        case CL_INVALID_GL_OBJECT: return "CL_INVALID_GL_OBJECT";
        case CL_INVALID_BUFFER_SIZE: return "CL_INVALID_BUFFER_SIZE";
        case CL_INVALID_MIP_LEVEL: return "CL_INVALID_MIP_LEVEL";
        case CL_INVALID_GLOBAL_WORK_SIZE: return "CL_INVALID_GLOBAL_WORK_SIZE";
        default: return "(unknown cl_error)";
    }
}

// ── SVM capability helpers ───────────────────────────────────────────────────
static inline cl_bitfield rixGetSVMCaps(cl_device_id dev) {
#ifdef CL_DEVICE_SVM_CAPABILITIES
    cl_bitfield caps = 0;
    cl_int e = clGetDeviceInfo(dev, CL_DEVICE_SVM_CAPABILITIES, sizeof(caps), &caps, NULL);
    if (e == CL_SUCCESS) return caps;
#endif
    return 0;
}

static inline int rixHasSVM(cl_device_id dev) {
    return rixGetSVMCaps(dev) != 0;
}

// ── Queue creation (1.2 vs 2.0+) ────────────────────────────────────────────
static inline cl_command_queue rixCreateQueue(cl_context ctx, cl_device_id dev, cl_int* out_err) {
    cl_int err = CL_SUCCESS;
    cl_command_queue q = NULL;
#if CL_TARGET_OPENCL_VERSION >= 200
    const cl_queue_properties props[] = { 0 }; // no properties
    q = clCreateCommandQueueWithProperties(ctx, dev, props, &err);
#else
    q = clCreateCommandQueue(ctx, dev, 0, &err);
#endif
    if (out_err) *out_err = err;
    return q;
}

// ── Program build helper (prints build log on failure) ──────────────────────
static inline cl_program rixBuildProgram(cl_context ctx, cl_device_id dev,
                                         const char* src, const char* options) {
    cl_int err = CL_SUCCESS;
    const char* sources[] = { src };
    const size_t lengths[] = { src ? strlen(src) : 0 };
    cl_program prog = clCreateProgramWithSource(ctx, 1, sources, lengths, &err);
    if (err != CL_SUCCESS) {
        fprintf(stderr, "clCreateProgramWithSource failed: %s\n", rixCLErrorName(err));
        return NULL;
    }
    err = clBuildProgram(prog, 1, &dev, options ? options : "", NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t log_sz = 0;
        clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_sz);
        char* logbuf = (char*)malloc(log_sz + 1);
        if (logbuf) {
            clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, log_sz, logbuf, NULL);
            logbuf[log_sz] = '\0';
            fprintf(stderr, "Program build log:\n%s\n", logbuf);
            free(logbuf);
        }
        fprintf(stderr, "clBuildProgram failed: %s\n", rixCLErrorName(err));
        clReleaseProgram(prog);
        return NULL;
    }
    return prog;
}

#endif // RETRYIX_CL_COMPAT_H
