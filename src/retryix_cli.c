// retryix_cli.c
// 簡易互動 CLI：用來測試 retryix_host + host_comm + module_descriptor
// 用法：
//   ./retryix_cli               # 使用內建 fallback kernel
//   ./retryix_cli my_kernel.cl  # 用外部 kernel 檔案

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "host_comm.h"
#include "module_descriptor.h"

// 從 retryix_host.c 導出的 API
int  retryix_init_minimal(void);
int  retryix_init_from_file(const char* kernel_path, const char* build_opts);
int  retryix_shutdown(void);
int  retryix_send_command(const char* message, char* response, size_t response_size);

// 小工具：送指令並列印回應
static void demo_exchange(const char* cmd) {
    char resp[1024] = {0};
    int r = retryix_send_command(cmd, resp, sizeof(resp));
    if (r == 0) {
        printf(">> %-12s | << %s\n", cmd, resp);
    } else {
        printf(">> %-12s | << (send failed)\n", cmd);
    }
}

int main(int argc, char** argv) {
    // 初始化通訊
    if (comm_init("RetryIXChannel") != COMM_SUCCESS) {
        fprintf(stderr, "comm_init failed\n");
        return 1;
    }

    // 初始化 Host
    int rc = 0;
    if (argc > 1) {
        rc = retryix_init_from_file(argv[1], "-cl-std=CL1.2");
    } else {
        rc = retryix_init_minimal();
    }
    if (rc != 0) {
        fprintf(stderr, "Host init failed: %d\n", rc);
        return 2;
    }

    // 先跑幾個內建測試
    demo_exchange("ping");
    demo_exchange("status");
    demo_exchange("whoami");
    demo_exchange("eval:1.0");

    // 進入互動 CLI
    printf("\n[RetryIX CLI] Commands: ping | status | whoami | eval:<f> | quit\n");
    char line[512];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t n = strlen(line);
        while (n && (line[n-1]=='\n' || line[n-1]=='\r')) line[--n] = '\0';
        if (!n) continue;
        if (strcmp(line, "quit") == 0) break;
        demo_exchange(line);
    }

    retryix_shutdown();
    comm_cleanup();
    return 0;
}
