#include "../include/retryix.h"
#include <stdio.h>

int main() {
    RetryIXDevice devices[16];
    int count = 0;
    int ret = retryixGetDeviceList(devices, 16, &count);

    if (ret == 0) {
        printf("Found %d OpenCL device(s).\n", count);
        for (int i = 0; i < count; ++i) {
            printf(" [%d] Name: %s\n", i, devices[i].name);
            printf("      Type: %lu\n", (unsigned long)devices[i].type);
            printf("      Global Memory: %.2f MB\n", (double)devices[i].global_mem_size / (1024 * 1024));
            printf("      OpenCL Version: %s\n\n", devices[i].opencl_version);
        }
    } else {
        printf("Failed to get device list.\n");
    }
    return 0;
}
