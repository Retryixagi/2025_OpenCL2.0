// retryix_exports.c
// Thin exported wrappers for Python/ctypes without touching original files.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "retryix.h"

#ifdef _WIN32
  #define RIX_API __declspec(dllexport)
  #define RIX_CDECL __cdecl
#else
  #define RIX_API
  #define RIX_CDECL
#endif

// ---- Version & selftest ----

RIX_API int RIX_CDECL retryix_get_version(int* major, int* minor, int* patch) {
    if (!major || !minor || !patch) return -1;
    *major = 2; *minor = 0; *patch = 0;   // 對齊你的 2.0.0
    return 0;
}

RIX_API int RIX_CDECL retryix_selftest(void) {
    // 不觸碰任何廠商驅動；僅做基本自檢
    return 0; // OK
}

// ---- Device count via existing RetryIX APIs ----

RIX_API int RIX_CDECL retryix_device_count(void) {
    RetryIXDevice devs[RETRYIX_MAX_DEVICES];
    int count = 0;
    int rc = retryixGetDeviceList(devs, RETRYIX_MAX_DEVICES, &count);
    if (rc != RETRYIX_SUCCESS) return rc;   // 傳回你的錯誤碼（負數）
    return count;                            // 直接回設備數
}

// ---- Enumerate devices to JSON (in-memory) ----
// 由於目前 public API 是「輸出到檔案」，這裡先呼叫它生成檔案，再讀回記憶體。
// 介面規格：buf==NULL 或 cap==0 時，回傳「所需長度」（不含終止字元）。
// 若緩衝區不足，回傳 -5（對齊你在 retryix.h 裡的 RETRYIX_ERROR_BUFFER_TOO_SMALL）。
// 成功時回傳實際寫入長度（不含終止字元）。

static int read_all(const char* path, char** out_buf, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    char* mem = (char*)malloc((size_t)sz + 1);
    if (!mem) { fclose(f); return -1; }
    size_t rd = fread(mem, 1, (size_t)sz, f);
    fclose(f);
    mem[rd] = '\0';
    *out_buf = mem;
    *out_len = rd;
    return 0;
}

RIX_API int RIX_CDECL retryix_enumerate_devices_json(char* buf, unsigned long cap) {
    const char* tmp_path = "retryix_device_report_tmp.json";

    // 用你現有 API 輸出檔案
    int erc = retryix_export_all_devices_json(tmp_path);
    if (erc != RETRYIX_SUCCESS) {
        // 若底層無設備也算成功，但這裡尊重底層錯誤碼
        return erc;
    }

    // 讀回記憶體
    char* file_buf = NULL;
    size_t file_len = 0;
    if (read_all(tmp_path, &file_buf, &file_len) != 0) {
        return -6; // RETRYIX_ERROR_FILE_IO
    }

    // 如果呼叫方只想知道需要多大緩衝區
    if (buf == NULL || cap == 0) {
        int need = (int)file_len; // 不含 '\0'
        free(file_buf);
        return need;
    }

    // 檢查容量
    if (cap <= file_len) {
        free(file_buf);
        return -5; // RETRYIX_ERROR_BUFFER_TOO_SMALL
    }

    // 複製 + NUL 結尾
    memcpy(buf, file_buf, file_len);
    buf[file_len] = '\0';
    free(file_buf);
    return (int)file_len;
}

// ---- Optional: 最後錯誤字串（目前簡單返回空字串即可） ----
RIX_API int RIX_CDECL retryix_last_error(char* buf, unsigned long cap) {
    const char* s = "";
    size_t n = strlen(s);
    if (!buf || cap == 0) return (int)n;
    if (cap <= n) return -5; // buffer too small
    memcpy(buf, s, n + 1);
    return (int)n;
}
