// retryix_host.c
// Unified RetryIX Host with module registration + host_comm wiring
// Fixes:
//  - retryix_send_command(): use in-thread loopback (no queue echo)
//  - add forward declarations to avoid implicit declaration warnings
//  - guard CL_TARGET_OPENCL_VERSION to avoid redefinition

#ifndef CL_TARGET_OPENCL_VERSION
#define CL_TARGET_OPENCL_VERSION 220
#endif

#include <CL/cl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "host_comm.h"
#include "module_descriptor.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// ===== Forward Declarations (avoid implicit declaration warnings) =====
EXPORT int retryix_init_from_source(const char* kernel_source, const char* build_opts);
EXPORT int retryix_init_from_file(const char* kernel_path, const char* build_opts);
EXPORT int retryix_init_minimal(void);
EXPORT cl_kernel retryix_create_kernel(const char* kernel_name);
EXPORT int retryix_execute_kernel(float* host_data, size_t count);
EXPORT int retryix_launch_1d(cl_kernel k, cl_mem arg0, size_t global);
EXPORT int retryix_host_receive_command(const char *input, char *response, size_t response_size);
EXPORT int retryix_send_command(const char* message, char* response, size_t response_size);
EXPORT int retryix_shutdown(void);

// ===== Common helpers =====
#define MAX_KERNELS 64

#define check_error(e, msg) \
    do { if ((e) != CL_SUCCESS) { \
        fprintf(stderr, "[RetryIX Host] OpenCL Error %d @ %s\n", (int)(e), (msg)); \
        return -1; } } while (0)

static cl_platform_id   g_platform = NULL;
static cl_device_id     g_device   = NULL;
static cl_context       g_context  = NULL;
static cl_command_queue g_queue    = NULL;
static cl_program       g_program  = NULL;

static cl_kernel        g_kernels[MAX_KERNELS] = {0};
static char             g_kernel_names[MAX_KERNELS][128] = {{0}};
static int              g_kernel_count = 0;

// ----- optional built-in fallback kernel (name: "test") -----
// does: data[i] = data[i] + 1.0f
static const char* kFallbackKernel =
"__kernel void test(__global float* data){\n"
"  size_t i = get_global_id(0);\n"
"  data[i] = data[i] + 1.0f;\n"
"}\n";

// ====== Simple in-process module registry (local minimal impl) ======
static RetryIXModuleDescriptor g_self_desc;
static int g_has_desc = 0;

static void gen_uuid_v4(char out[64]) {
    // 非密碼學用途的簡易 UUIDv4 產生器（demo）
    unsigned int r[4];
    srand((unsigned)time(NULL) ^ 0xA5A5A5A5u);
    for (int i=0;i<4;i++) r[i] = (unsigned)rand();
    snprintf(out, 64, "%08x-%04x-%04x-%04x-%04x%08x",
             r[0],
             (r[1]>>16) & 0xFFFF,
             ((r[1] & 0x0FFF) | 0x4000),      // version 4
             ((r[2] & 0x3FFF) | 0x8000),      // variant 10
             (r[2]>>16) & 0xFFFF,
             r[3]);
}

int retryix_register_module(const RetryIXModuleDescriptor* desc) {
    if (!desc) return -1;
    g_self_desc = *desc;
    g_has_desc = 1;
    fprintf(stdout,
        "[RetryIX Host] Module registered: name=%s, uuid=%s, profile=%s, blockchain=%d\n",
        g_self_desc.module_name, g_self_desc.uuid, g_self_desc.semantic_profile,
        g_self_desc.supports_blockchain);
    return 0;
}

int retryix_query_module(char* buffer, size_t max_len) {
    if (!g_has_desc || !buffer || max_len==0) return -1;
    snprintf(buffer, max_len,
        "{\"module_name\":\"%s\",\"uuid\":\"%s\",\"semantic_profile\":%s,\"supports_blockchain\":%d}",
        g_self_desc.module_name, g_self_desc.uuid,
        g_self_desc.semantic_profile[0] ? g_self_desc.semantic_profile : "\"\"",
        g_self_desc.supports_blockchain);
    return 0;
}

static int retryix_register_self(void) {
    RetryIXModuleDescriptor d = {0};
    snprintf(d.module_name, sizeof(d.module_name), "RetryIX Host");
    gen_uuid_v4(d.uuid);
    snprintf(d.semantic_profile, sizeof(d.semantic_profile),
             "{\"type\":\"AGI\",\"sub\":\"SVM\",\"cap\":\"ZeroCopy\"}");
    d.supports_blockchain = 0;
    return retryix_register_module(&d);
}

// ===== Utility =====
static char* read_text_file(const char* path, long* out_size) {
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t n = fread(buf, 1, sz, fp);
    buf[n] = '\0';
    fclose(fp);
    if (out_size) *out_size = (long)n;
    return buf;
}

static void print_build_log(cl_program prog, cl_device_id dev, const char* when) {
    size_t log_size = 0;
    clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    if (log_size > 1) {
        char* log = (char*)malloc(log_size);
        if (log) {
            clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
            fprintf(stderr, "[RetryIX Host] Build Log (%s):\n%s\n", when, log);
            free(log);
        }
    }
}

// ===== Device & Context init =====
static int pick_first_device(cl_device_type prefer, cl_platform_id* out_pf, cl_device_id* out_dev) {
    cl_int err;
    cl_uint np = 0;
    err = clGetPlatformIDs(0, NULL, &np);
    if (err != CL_SUCCESS || np == 0) { fprintf(stderr, "No OpenCL platforms.\n"); return -1; }
    cl_platform_id* pf = (cl_platform_id*)malloc(sizeof(cl_platform_id)*np);
    if (!pf) return -1;
    err = clGetPlatformIDs(np, pf, NULL);
    if (err != CL_SUCCESS) { free(pf); return -1; }

    for (cl_uint i = 0; i < np; ++i) {
        cl_uint nd = 0;
        if (clGetDeviceIDs(pf[i], prefer, 0, NULL, &nd) == CL_SUCCESS && nd > 0) {
            cl_device_id* devs = (cl_device_id*)malloc(sizeof(cl_device_id)*nd);
            if (!devs) { free(pf); return -1; }
            err = clGetDeviceIDs(pf[i], prefer, nd, devs, NULL);
            if (err == CL_SUCCESS && nd > 0) {
                *out_pf = pf[i]; *out_dev = devs[0];
                free(devs); free(pf);
                return 0;
            }
            free(devs);
        }
        if (clGetDeviceIDs(pf[i], CL_DEVICE_TYPE_ALL, 0, NULL, &nd) == CL_SUCCESS && nd > 0) {
            cl_device_id* devs = (cl_device_id*)malloc(sizeof(cl_device_id)*nd);
            if (!devs) { free(pf); return -1; }
            err = clGetDeviceIDs(pf[i], CL_DEVICE_TYPE_ALL, nd, devs, NULL);
            if (err == CL_SUCCESS && nd > 0) {
                *out_pf = pf[i]; *out_dev = devs[0];
                free(devs); free(pf);
                return 0;
            }
            free(devs);
        }
    }
    free(pf);
    fprintf(stderr, "No suitable OpenCL device.\n");
    return -1;
}

// ===== Public API =====
EXPORT int retryix_init_minimal(void) {
    return retryix_init_from_source(kFallbackKernel, "-cl-std=CL1.2");
}

EXPORT int retryix_init_from_source(const char* kernel_source,
                                    const char* build_opts /* can be NULL */) {
    cl_int err;

    if (g_context || g_program) return 0;

    if (pick_first_device(CL_DEVICE_TYPE_GPU, &g_platform, &g_device) != 0) return -1;

    g_context = clCreateContext(NULL, 1, &g_device, NULL, NULL, &err);
    check_error(err, "clCreateContext");

    const cl_queue_properties props[] = { 0 };
    g_queue = clCreateCommandQueueWithProperties(g_context, g_device, props, &err);
    check_error(err, "clCreateCommandQueueWithProperties");

    g_program = clCreateProgramWithSource(g_context, 1, &kernel_source, NULL, &err);
    check_error(err, "clCreateProgramWithSource");

    err = clBuildProgram(g_program, 1, &g_device, build_opts ? build_opts : "", NULL, NULL);
    if (err != CL_SUCCESS) {
        print_build_log(g_program, g_device, "from_source");
        check_error(err, "clBuildProgram(from_source)");
    }

    g_kernel_count = 0;
    memset(g_kernels, 0, sizeof(g_kernels));
    memset(g_kernel_names, 0, sizeof(g_kernel_names));

    // 註冊自身（可選）
    retryix_register_self();
    return 0;
}

EXPORT int retryix_init_from_file(const char* kernel_path,
                                  const char* build_opts /* can be NULL */) {
    long sz = 0;
    char* src = read_text_file(kernel_path, &sz);
    if (!src) {
        perror("[RetryIX Host] fopen kernel_path");
        return -1;
    }
    int rc = retryix_init_from_source(src, build_opts);
    free(src);
    return rc;
}

EXPORT cl_kernel retryix_create_kernel(const char* kernel_name) {
    if (!g_program) { fprintf(stderr, "Program not built.\n"); return NULL; }
    if (g_kernel_count >= MAX_KERNELS) { fprintf(stderr, "Kernel cache full.\n"); return NULL; }

    cl_int err;
    cl_kernel k = clCreateKernel(g_program, kernel_name, &err);
    if (err != CL_SUCCESS || !k) {
        print_build_log(g_program, g_device, "create_kernel_failed");
        fprintf(stderr, "clCreateKernel('%s') failed: %d\n", kernel_name, (int)err);
        return NULL;
    }
    g_kernels[g_kernel_count] = k;
    strncpy(g_kernel_names[g_kernel_count], kernel_name, sizeof(g_kernel_names[0])-1);
    g_kernel_count++;
    return k;
}

static cl_kernel ensure_test_kernel(void) {
    for (int i=0;i<g_kernel_count;i++){
        if (strcmp(g_kernel_names[i], "test")==0) return g_kernels[i];
    }
    return retryix_create_kernel("test");
}

EXPORT int retryix_execute_kernel(float* host_data, size_t count) {
    if (!g_context || !g_queue) return -3;

    cl_int err;
    cl_kernel k = ensure_test_kernel();
    if (!k) return -7;

    cl_mem buffer = clCreateBuffer(g_context,
                                   CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   sizeof(float) * count, host_data, &err);
    if (!buffer || err != CL_SUCCESS) return -10;

    err = clSetKernelArg(k, 0, sizeof(cl_mem), &buffer);
    if (err != CL_SUCCESS) { clReleaseMemObject(buffer); return -11; }

    size_t global = count;
    err = clEnqueueNDRangeKernel(g_queue, k, 1, NULL, &global, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) { clReleaseMemObject(buffer); return -12; }

    clFinish(g_queue);

    err = clEnqueueReadBuffer(g_queue, buffer, CL_TRUE, 0,
                              sizeof(float) * count, host_data, 0, NULL, NULL);
    if (err != CL_SUCCESS) { clReleaseMemObject(buffer); return -13; }

    clReleaseMemObject(buffer);
    return 0;
}

EXPORT int retryix_launch_1d(cl_kernel k, cl_mem arg0, size_t global) {
    if (!g_queue) return -3;
    cl_int err = clSetKernelArg(k, 0, sizeof(cl_mem), &arg0);
    if (err != CL_SUCCESS) return -11;
    err = clEnqueueNDRangeKernel(g_queue, k, 1, NULL, &global, NULL, 0, NULL, NULL);
    if (err != CL_SUCCESS) return -12;
    clFinish(g_queue);
    return 0;
}

// ===== Comms glue (封包式對接 host_comm) =====
EXPORT int retryix_host_receive_command(const char *input,
                                        char *response,
                                        size_t response_size) {
    if (!input || !response || response_size==0) return -1;

    if (strcmp(input, "ping") == 0) {
        snprintf(response, response_size, "pong");
    } else if (strcmp(input, "status") == 0) {
        snprintf(response, response_size, "RetryIX v2.0 Host Ready");
    } else if (strncmp(input, "eval:", 5) == 0) {
        float val = 0.0f;
        (void)sscanf(input + 5, "%f", &val);
        float data[1] = { val };
        int ret = retryix_execute_kernel(data, 1);
        if (ret == 0) snprintf(response, response_size, "result=%.3f", data[0]);
        else          snprintf(response, response_size, "error=%d", ret);
    } else if (strcmp(input, "whoami") == 0) {
        char buf[256];
        if (retryix_query_module(buf, sizeof(buf)) == 0)
            snprintf(response, response_size, "%s", buf);
        else
            snprintf(response, response_size, "no_module");
    } else {
        snprintf(response, response_size, "unknown command");
    }
    return 0;
}

// Fixed: in-thread loopback (no queue echo). Replace later with real IPC if needed.
EXPORT int retryix_send_command(const char* message,
                                char* response,
                                size_t response_size) {
    if (!message || !response || response_size==0) return -1;

    char resp[1024] = {0};
    int rc = retryix_host_receive_command(message, resp, sizeof(resp));
    if (rc != 0) return rc;

    snprintf(response, response_size, "%s", resp);
    return 0;
}

EXPORT int retryix_shutdown(void) {
    for (int i = 0; i < MAX_KERNELS; ++i) {
        if (g_kernels[i]) { clReleaseKernel(g_kernels[i]); g_kernels[i]=NULL; }
    }
    g_kernel_count = 0;
    if (g_program) { clReleaseProgram(g_program); g_program=NULL; }
    if (g_queue)   { clReleaseCommandQueue(g_queue); g_queue=NULL; }
    if (g_context) { clReleaseContext(g_context); g_context=NULL; }
    g_device = NULL; g_platform = NULL;
    memset(g_kernel_names, 0, sizeof(g_kernel_names));
    return 0;
}
