# RetryIX Universal Compute Platform v2.0.0

> 通用計算抽象平台（中英文說明） / Universal Compute Abstraction Layer (ZH/EN)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Status](https://img.shields.io/badge/status-2.0.0-brightgreen)

---

## 📌 Overview / 概述

**中文**
RetryIX 是一個通用計算抽象層（UCAL），支援跨平台 GPU 與記憶體模組整合，並針對 **SVM（共享虛擬記憶體）**、**原子操作**、**分散式記憶體** 等功能提供一致介面。它運行於 **OpenCL 2.0+**，並支援 AMD / Intel / NVIDIA 等廠商的擴展與最佳化。

**English**
RetryIX is a **Universal Compute Abstraction Layer (UCAL)** that integrates cross‑vendor GPU and memory modules. It provides a unified interface for **Shared Virtual Memory (SVM)**, **atomic operations**, and **memory‑optimized computation**. It runs on **OpenCL 2.0+** and supports vendor‑specific extensions for AMD / Intel / NVIDIA.

---

## 📦 Included Components / 安裝內容

* `retryix.dll` — Core runtime DLL（核心運算介面）
* `retryix.h` — C/C++ header（標頭檔）
* `libretryix.a` — Static library（靜態連結庫）
* `retryix_service.exe` — Background service（後端管理服務）
* `retryix_editor.exe` — Kernel editor (optional)（內核編輯器，可選）
* `retryix.ico` — Desktop shortcut icon（捷徑圖示）
* `uninstall.exe` — Uninstaller（移除工具）
* `config.xml` — Configuration file（配置檔）
* `README.md` — This document（本文件）

---

## ⚙️ System Requirements / 系統需求

**Minimum / 最低需求**

* Windows 10 / 11 / Server 2019+
* OpenCL **1.2+** compatible GPU driver（AMD / Intel / NVIDIA）
* Disk space ≥ **150MB**
* Administrator privileges（安裝服務與註冊表需要系統管理員）

**Recommended / 建議環境**

* AMD RDNA (GFX10+) / Intel Xe / NVIDIA GTX 10‑series+
* OpenCL **2.0** with **FINE\_GRAIN\_BUFFER** & atomics
* **8GB+** system RAM
* **Python 3.11+** (if using Python bindings)

---

## 🚀 Installation / 安裝

### Automatic / 自動安裝

1. Run `RetryIX-2.0.0-Setup.exe`
2. Follow the wizard
3. Windows service & registry entries are registered automatically
4. Reboot after installation

### Manual / 手動安裝

1. Extract files to: `C:\Program Files\RetryIX\`
2. Register DLL (Administrator): `regsvr32 retryix.dll`
3. Import registry: `regedit /s retryix_complete_registry.reg`
4. Start service: `net start RetryIXService`

---

## 🧪 Quick Start / 快速開始

### C API Example

```c
#include "retryix.h"

int main(void) {
    // Initialize RetryIX context
    retryix_svm_context_t* ctx = retryix_svm_create_context(context, device);

    // Allocate Shared Virtual Memory (SVM)
    float* data = (float*)retryix_svm_alloc(
        ctx,
        1024 * sizeof(float),
        RETRYIX_SVM_FLAG_READ_WRITE
    );

    for (int i = 0; i < 1024; ++i) {
        data[i] = i * 3.14f; // CPU‑side edit
    }

    // Zero‑copy GPU kernel execution
    run_gpu_kernel(data);

    // Cleanup
    retryix_svm_free(ctx, data);
    retryix_svm_destroy_context(ctx);
    return 0;
}
```

### Python Binding Example

```python
import ctypes

# Load dynamic library
retryix = ctypes.CDLL(r"C:\\Program Files\\RetryIX\\bin\\retryix.dll")

# (Pseudo) usage — fill in actual signatures via ctypes
# devices = retryix.retryix_get_device_list()
# print(f"Found {len(devices)} compute devices")
# buf = retryix.svm_alloc(1024 * 4)
# result = retryix.execute_kernel(b"vector_add", buf)
```

> ⚠️ The Python snippet illustrates loading the DLL; please align `ctypes` function signatures with your exported API.

---

## 🧰 Diagnostics & Troubleshooting / 診斷與疑難排解

**CLI Tools / 指令**

* `retryix_host.exe --test` — Compatibility test（相容性測試）
* `retryix_host.exe --info` — Hardware info（硬體資訊）
* `retryix_host.exe --bench` — Benchmarks（效能基準）

**FAQ / 常見問題**

* **Install error: “OpenCL runtime not found”** → Install latest GPU driver (AMD Adrenalin / NVIDIA GeForce / Intel Arc)
* **Runtime “SVM allocation failed”** → Check available system RAM; reduce SVM allocation size
* **Old GPUs show lower performance** → RetryIX falls back to compatibility mode — expected behavior

---

## 🧑‍💻 Developer Info / 開發者資訊

**Build Requirements / 編譯需求**

* MinGW‑w64 8.1.0+ or Visual Studio 2019+
* OpenCL Headers 2.0+
* CMake 3.16+ (optional)

**Docs & Links / 文件連結**

* API Reference: [https://docs.retryixagi.com/api/](https://docs.retryixagi.com/api/)
* Examples: [https://github.com/retryix/examples](https://github.com/retryix/examples)
* Community: [https://community.retryixagi.com](https://community.retryixagi.com)

---

## 📄 License / 授權條款

Licensed under the **MIT License**. See [`LICENSE`](LICENSE) for details.
本專案採用 **MIT** 授權，詳見 `LICENSE` 檔案。

---

## 🆘 Support / 技術支援

* Website: [https://retryixagi.com](https://retryixagi.com)
* Docs: [https://docs.retryixagi.com](https://docs.retryixagi.com)
* Email: [support@retryixagi.com](mailto:support@retryixagi.com)
* Issues: [https://github.com/retryix/issues](https://github.com/retryix/issues)
* Discord: [https://discord.gg/retryix](https://discord.gg/retryix)

---

## 🔄 Version History / 版本記錄

* **v2.0.0 (2025‑08‑26)**

  * New universal SVM memory manager
  * Cross‑vendor atomics
  * Smart kernel compilation strategy
  * Full Windows service integration

* **v1.5.0 (2025‑03‑15)**

  * Base OpenCL abstraction
  * Device discovery & capability query

* **v1.0.0 (2025‑01‑01)**

  * Initial release (PoC)

---

## ⚠️ Important Notes / 重要注意事項

**中文**

* 安裝與運行需系統管理員權限
* 首次啟動包含硬體相容性檢測，可能花費較久時間
* 在 AMD GPU 上可能需要手動啟用進階功能
* 強烈建議於生產前充分測試

**English**

* Administrator privileges required
* First run includes hardware compatibility detection
* Advanced features on AMD GPUs may require manual enabling
* Thorough testing is recommended before production

---

## 🙏 Acknowledgments / 致謝

Thanks to all community contributors — especially **Ice Xu** and the **RetryIX Foundation** team.
Special appreciation to the creators and maintainers of **OpenCL** 
Apple, 2008;
**Khronos Group** ongoing
for building an open, cross‑platform compute ecosystem.

---

© 2025 RetryIX Foundation. All rights reserved.
