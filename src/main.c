// main.c
#include <stdio.h>
#include "retryix.h"

int main(void) {
    printf("🚀 RetryIX Platform Device Inspector Launched\n");

    RetryIXDevice devices[RETRYIX_MAX_DEVICES];
    int count = 0;

    int status = retryixGetDeviceList(devices, RETRYIX_MAX_DEVICES, &count);
    if (status != RETRYIX_SUCCESS) {
        printf("❌ Failed to enumerate devices (error %d)\n", status);
        return 1;
    }

    printf("✅ Found %d device(s)\n", count);
    for (int i = 0; i < count; i++) {
        printf("[%d] %s\n", i, devices[i].name);
        printf("    Type: %lu\n", (unsigned long)devices[i].type);
        printf("    Global Mem: %llu MB\n",
               (unsigned long long)devices[i].global_mem_size / (1024ULL * 1024ULL));
        printf("    OpenCL: %s\n", devices[i].opencl_version);
        printf("    SVM Caps: %llu\n", (unsigned long long)devices[i].svm_capabilities);
        printf("    Extensions: %s\n\n", devices[i].extensions);
    }

    status = retryix_export_all_devices_json("device_report.json");
    if (status == RETRYIX_SUCCESS) {
        printf("📦 Exported device report to device_report.json\n");
    } else {
        printf("❌ Failed to export device report (error %d)\n", status);
    }

    return 0;
}
