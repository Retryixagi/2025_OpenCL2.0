# 2025_OpenCL2.0
All new API
===========================================
RetryIX Universal Compute Platform v2.0.0
通用計算抽象平台 (中英文版說明文件)
===========================================
📅 發佈日期 / Release Date: 2025-08-26
👤 開發者 / Developer: RetryIX Foundation
🌐 官方網站 / Website: https://retryixagi.com
📖 文件 / Documentation: https://docs.retryixagi.com
──────────────────────────────────────────
📌 概述 / Overview
──────────────────────────────────────────
[中文]  
RetryIX 是一個通用計算抽象層（UCAL），支援跨平台 GPU 與記憶體模組整合，並針對 SVM、原子操作、分散式記憶體等功能提供一致介面。
它可在 OpenCL 2.0 以上環境中運作，並支援不同硬體廠商（AMD / Intel / NVIDIA）的兼容與優化擴展。

[English]  
RetryIX is a Universal Compute Abstraction Layer (UCAL) supporting cross-vendor GPU and memory integrations.  
It provides a unified interface for SVM (Shared Virtual Memory), atomic operations, and memory-optimized computation.  
It runs on top of OpenCL 2.0+ and supports vendor-specific extensions for AMD / Intel / NVIDIA.

──────────────────────────────────────────
📦 安裝內容 / Included Components
──────────────────────────────────────────
✔ retryix.dll         - 核心運算介面 (Core Runtime DLL)  
✔ retryix.h           - C/C++ 標頭檔 (Header File)
✔ libretryix.a        - 靜態連結庫 (Static Library)
✔ retryix_service.exe - 後端管理服務 (Background Service)  
✔ retryix_editor.exe  - 內核編輯器 (Optional Kernel Editor)  
✔ retryix.ico         - 桌面捷徑圖示  
✔ uninstall.exe       - 移除工具  
✔ config.xml          - 配置檔案 (Configuration File)
✔ README.txt          - 說明文件（本檔案）

──────────────────────────────────────────
⚙️ 系統需求 / System Requirements
──────────────────────────────────────────
[最低需求 / Minimum Requirements]
- Windows 10 / 11 / Server 2019+
- OpenCL 1.2+ compatible GPU driver (AMD/Intel/NVIDIA)
- 安裝空間至少 150MB (At least 150MB free space)
- 需要管理員權限安裝服務與註冊表 (Administrator privileges required)

[建議環境 / Recommended Environment]
- AMD RDNA (GFX10+) 或 Intel Xe 或 NVIDIA GTX 10 系列以上
- 支援 OpenCL 2.0 FINE_GRAIN_BUFFER 與原子操作
- 8GB+ 系統記憶體 (System RAM)
- Python 3.11+ (若需要使用 Python 綁定)

──────────────────────────────────────────
🚀 快速安裝 / Quick Installation
──────────────────────────────────────────
[自動安裝 / Automatic Installation]
1. 執行 RetryIX-2.0.0-Setup.exe
2. 依照安裝精靈指示完成安裝
3. 系統將自動註冊 Windows 服務和註冊表項目
4. 安裝完成後重新啟動電腦

[手動安裝 / Manual Installation]
1. 解壓縮檔案到：C:\Program Files\RetryIX\
2. 以系統管理員身分執行：regsvr32 retryix.dll
3. 匯入註冊表：regedit /s retryix_complete_registry.reg
4. 啟動服務：net start RetryIXService

──────────────────────────────────────────
📚 使用範例 / Example Usage
──────────────────────────────────────────
[C API 範例 / C API Example]
```c
#include "retryix.h"

int main() {
    // 初始化 RetryIX 系統
    retryix_svm_context_t* ctx = retryix_svm_create_context(context, device);
    
    // 分配共享記憶體
    float* data = (float*)retryix_svm_alloc(ctx, 1024 * sizeof(float), 
                                           RETRYIX_SVM_FLAG_READ_WRITE);
    
    // CPU 端操作
    for (int i = 0; i < 1024; i++) {
        data[i] = i * 3.14f;
    }
    
    // GPU 端處理（零拷貝）
    run_gpu_kernel(data);
    
    // 清理資源
    retryix_svm_free(ctx, data);
    retryix_svm_destroy_context(ctx);
    return 0;
}
```

[Python 綁定範例 / Python Binding Example]
```python
import ctypes
import numpy as np

# 載入 RetryIX 動態庫
retryix = ctypes.CDLL('C:/Program Files/RetryIX/bin/retryix.dll')

# 檢測可用設備
devices = retryix.retryix_get_device_list()
print(f"Found {len(devices)} compute devices")

# 分配 SVM 記憶體並執行計算
data = retryix.svm_alloc(1024 * 4)  # 1024 floats
result = retryix.execute_kernel("vector_add", data)
```

──────────────────────────────────────────
🔧 故障排除 / Troubleshooting
──────────────────────────────────────────
[常見問題 / Common Issues]

Q: 安裝時出現「找不到 OpenCL 運行時」錯誤
A: 請先安裝最新版本的 GPU 驅動程式 (AMD Adrenalin / NVIDIA GeForce / Intel Arc)

Q: Installation fails with "OpenCL runtime not found"
A: Please install the latest GPU drivers first

Q: 程式執行時顯示 "SVM allocation failed"
A: 檢查系統記憶體是否足夠，或降低 SVM 記憶體配置大小

Q: Program crashes with "SVM allocation failed"
A: Check available system memory or reduce SVM allocation size

Q: 在舊版 GPU 上執行效能較差
A: RetryIX 會自動降級到兼容模式，這是正常現象

Q: Poor performance on legacy GPUs
A: RetryIX automatically falls back to compatibility mode - this is expected

[診斷工具 / Diagnostic Tools]
- retryix_host.exe --test    # 執行系統相容性測試
- retryix_host.exe --info    # 顯示硬體資訊
- retryix_host.exe --bench   # 執行效能基準測試

──────────────────────────────────────────
🛠️ 開發者資訊 / Developer Information
──────────────────────────────────────────
[編譯需求 / Build Requirements]
- MinGW-w64 8.1.0+ 或 Visual Studio 2019+
- OpenCL Headers 2.0+
- CMake 3.16+ (可選)

[API 文件 / API Documentation]
- 完整的 API 參考：https://docs.retryixagi.com/api/
- 範例代碼庫：https://github.com/retryix/examples
- 社群論壇：https://community.retryixagi.com

[授權條款 / License]
RetryIX Universal Compute Platform 採用 MIT 開源授權條款。
詳細授權內容請參閱 LICENSE.txt 檔案。

The RetryIX Universal Compute Platform is licensed under the MIT License.
See LICENSE.txt for full license terms.

──────────────────────────────────────────
📞 技術支援 / Technical Support
──────────────────────────────────────────
🌐 官方網站：https://retryixagi.com
📧 技術支援：support@retryixagi.com
📖 說明文件：https://docs.retryixagi.com
🐛 問題回報：https://github.com/retryix/issues
💬 社群討論：https://discord.gg/retryix

──────────────────────────────────────────
🔄 版本更新記錄 / Version History
──────────────────────────────────────────
v2.0.0 (2025-08-26)
+ 新增通用 SVM 記憶體管理器
+ 支援跨廠商原子操作
+ 實作智能內核編譯策略
+ 完整的 Windows 服務整合
+ Added universal SVM memory manager
+ Cross-vendor atomic operations support
+ Intelligent kernel compilation strategies
+ Complete Windows service integration

v1.5.0 (2025-03-15)
+ 基礎 OpenCL 抽象層實作
+ 設備檢測與能力查詢
+ Basic OpenCL abstraction layer
+ Device detection and capability probing

v1.0.0 (2025-01-01)
+ 初版發布，概念驗證
+ Initial release, proof of concept

──────────────────────────────────────────
⚠️ 重要注意事項 / Important Notes
──────────────────────────────────────────
[中文]
- 本軟體需要管理員權限才能正常安裝和運行
- 首次執行時會進行硬體相容性檢測，可能需要較長時間
- 在 AMD GPU 上可能需要手動啟用某些進階功能
- 建議在生產環境使用前先進行充分測試

[English]
- Administrator privileges required for installation and operation
- First run includes hardware compatibility detection (may take time)
- AMD GPU users may need to manually enable advanced features
- Thorough testing recommended before production deployment

──────────────────────────────────────────
🙏 致謝 / Acknowledgments
──────────────────────────────────────────
感謝所有參與 RetryIX 開發和測試的社群成員，特別感謝本系統原始設計者 Ice Xu 先生與 RetryIX Foundation 團隊。

我們亦向 OpenCL 標準的原始制定者與推動者表達最深的敬意，特別是 Apple Inc. 在 2008 年提出此標準的貢獻，以及 Khronos Group 長年推動其跨平台、開放性的努力。正因有他們的前瞻性與堅持，RetryIX 才能站在開放並行運算的基石上向前邁進。

本專案致力於打破計算硬體的人為壁壘，促進開放標準的發展。

Thanks to all community members who contributed to the development and testing of RetryIX, especially Mr. Ice Xu, the original system architect, and the RetryIX Foundation team.

We also express our deepest appreciation to the original creators and maintainers of the OpenCL standard, especially Apple Inc. for proposing it in 2008, and the Khronos Group for their continuous work in building an open, cross-platform compute ecosystem. Their visionary efforts laid the foundation upon which RetryIX continues to advance.

This project is dedicated to breaking down artificial barriers in compute hardware and promoting open standards in parallel computing.


===========================================
Copyright (c) 2025 RetryIX Foundation
All rights reserved.
===========================================
