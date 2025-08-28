#include <stdio.h>
#include <stdlib.h>

// 用 wrapper 的原型（可放進 retryix.h，或在此重宣）
#ifdef _WIN32
  #define RIX_API __declspec(dllimport)
  #define RIX_CDECL __cdecl
#else
  #define RIX_API
  #define RIX_CDECL
#endif

RIX_API int RIX_CDECL retryix_get_version(int*, int*, int*);
RIX_API int RIX_CDECL retryix_selftest(void);
RIX_API int RIX_CDECL retryix_device_count(void);
RIX_API int RIX_CDECL retryix_enumerate_devices_json(char* buf, unsigned long cap);

int main(void) {
    int maj=0,min=0,pat=0;
    if (retryix_get_version(&maj,&min,&pat)==0) {
        printf("RetryIX v%d.%d.%d\n", maj,min,pat);
    }

    if (retryix_selftest()==0) {
        printf("Selftest: OK\n");
    }

    int n = retryix_device_count();
    printf("Device count: %d\n", n);

    int need = retryix_enumerate_devices_json(NULL, 0);
    if (need > 0) {
        char *buf = (char*)malloc(need+1);
        if (buf) {
            int wrote = retryix_enumerate_devices_json(buf, need+1);
            if (wrote >= 0) {
                printf("Devices JSON:\n%s\n", buf);
            }
            free(buf);
        }
    }
    return 0;
}
