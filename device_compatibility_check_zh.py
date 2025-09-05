import pyopencl as cl
import ctypes
import json
import os
import winreg

def get_dll_path():
    """動態查找 RetryIX DLL 路徑"""
    print("[動態追蹤] 開始查找 DLL 路徑...")
    
    # 方法1: 查詢註冊表
    try:
        version_key = r"SOFTWARE\\RetryIX\\VersionHistory"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, version_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            current_version, _ = winreg.QueryValueEx(key, "CurrentVersion")
            print(f"[動態追蹤] 找到版本: {current_version}")
            
            # 查詢實際 DLL 路徑
            dll_key = fr"SOFTWARE\\RetryIX\\MemoryRAID\\v{current_version}"
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, dll_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as dll_reg:
                dll_path, _ = winreg.QueryValueEx(dll_reg, "DllPath")
                if os.path.exists(dll_path):
                    print(f"[動態追蹤] 註冊表路徑有效: {dll_path}")
                    return dll_path
                else:
                    print(f"[動態追蹤] 註冊表路徑無效: {dll_path}")
    except Exception as e:
        print(f"[動態追蹤] 註冊表查詢失敗: {e}")
    
    # 方法2: 常見安裝路徑
    common_paths = [
        r"C:\Program Files\RetryIX\bin\retryix.dll",
        r"C:\Program Files (x86)\RetryIX\bin\retryix.dll",
        r"C:\RetryIX\bin\retryix.dll",
        r"F:\opencl\src\retryix.dll",  # 你原來的路徑
    ]
    
    for path in common_paths:
        if os.path.exists(path):
            print(f"[動態追蹤] 在常見路徑找到: {path}")
            return path
    
    # 方法3: 在腳本同目錄查找
    script_dir = os.path.dirname(os.path.abspath(__file__))
    local_dll = os.path.join(script_dir, "retryix.dll")
    if os.path.exists(local_dll):
        print(f"[動態追蹤] 在腳本目錄找到: {local_dll}")
        return local_dll
    
    # 方法4: 在當前工作目錄查找
    cwd_dll = os.path.join(os.getcwd(), "retryix.dll")
    if os.path.exists(cwd_dll):
        print(f"[動態追蹤] 在工作目錄找到: {cwd_dll}")
        return cwd_dll
    
    raise FileNotFoundError("找不到 retryix.dll！請確認 RetryIX 已正確安裝")

# 動態載入 DLL
dll_path = get_dll_path()
print("載入 DLL 路徑：", dll_path)
print("檔案大小：", os.path.getsize(dll_path))
svm = ctypes.CDLL(dll_path)
print("retryix_query_all_resources in dir(svm):", "retryix_query_all_resources" in dir(svm))

class RetryIXSVMContext(ctypes.Structure):
    pass

# 定義 API
svm.retryix_svm_create_context.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
svm.retryix_svm_create_context.restype = ctypes.POINTER(RetryIXSVMContext)
svm.retryix_svm_destroy_context.argtypes = [ctypes.POINTER(RetryIXSVMContext)]
svm.retryix_svm_destroy_context.restype = None
svm.retryix_svm_alloc.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_size_t, ctypes.c_int]
svm.retryix_svm_alloc.restype = ctypes.c_void_p
svm.retryix_svm_free.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_void_p]
svm.retryix_svm_free.restype = ctypes.c_int

# === 新增: 呼叫核心 atomic kernel 測試 ===
svm.retryix_kernel_atomic_add_demo.argtypes = []
svm.retryix_kernel_atomic_add_demo.restype = None

def run_atomic_kernel_demo():
    print("[Python] 呼叫 retryix_kernel_atomic_add_demo() ...")
    svm.retryix_kernel_atomic_add_demo()
    print("[Python] atomic_add demo 執行完畢。")

def run_svm_demo():
    print("[SVM Demo] 開始...")
    # pyopencl 建立 context/device/queue
    platform = cl.get_platforms()[0]
    device = platform.get_devices()[0]
    print(f"[SVM Demo] 使用平台: {platform.name}")
    print(f"[SVM Demo] 使用裝置: {device.name}")
    
    ctx = cl.Context([device])
    queue = cl.CommandQueue(ctx)
    cl_context_ptr = ctypes.c_void_p(ctx.int_ptr)
    cl_device_id_ptr = ctypes.c_void_p(device.int_ptr)

    # 建立 SVM context
    print("[SVM Demo] 建立 SVM context...")
    svm_ctx = svm.retryix_svm_create_context(cl_context_ptr, cl_device_id_ptr)

    # 分配 SVM 記憶體
    print("[SVM Demo] 分配 SVM 記憶體...")
    RETRYIX_SVM_FINE_GRAIN = 2
    RETRYIX_SVM_ATOMIC = 0x8
    buf = svm.retryix_svm_alloc(svm_ctx, 4096, RETRYIX_SVM_FINE_GRAIN | RETRYIX_SVM_ATOMIC)
    print(f"SVM buffer allocated at: 0x{buf:x}")

    # 釋放 SVM 記憶體
    print("[SVM Demo] 釋放 SVM 記憶體...")
    svm.retryix_svm_free(svm_ctx, buf)
    print("SVM buffer freed.")

    # 銷毀 SVM context
    print("[SVM Demo] 銷毀 SVM context...")
    svm.retryix_svm_destroy_context(svm_ctx)
    print("SVM context destroyed.")

def query_atomic_svm_capabilities():
    print("[能力查詢] 開始查詢 OpenCL SVM 和原子操作能力...")
    platforms = cl.get_platforms()
    for p in platforms:
        print(f"平台: {p.name}")
        for d in p.get_devices():
            print(f"  裝置: {d.name}")
            exts = d.extensions.split()
            print(f"    Extensions 數量: {len(exts)}")
            
            try:
                svm_caps = d.get_info(cl.device_info.SVM_CAPABILITIES)
                print(f"    SVM Capabilities: 0x{svm_caps:x}")
                if svm_caps & cl.device_svm_capabilities.FINE_GRAIN_BUFFER:
                    print("      ✅ 支援 FINE_GRAIN_BUFFER")
                if svm_caps & cl.device_svm_capabilities.FINE_GRAIN_SYSTEM:
                    print("      ✅ 支援 FINE_GRAIN_SYSTEM")
                if svm_caps & cl.device_svm_capabilities.ATOMICS:
                    print("      ✅ 支援 ATOMICS")
                if svm_caps & cl.device_svm_capabilities.COARSE_GRAIN_BUFFER:
                    print("      ✅ 支援 COARSE_GRAIN_BUFFER")
            except Exception as e:
                print(f"    (無法查詢 SVM 能力: {e})")
                
            atomics = [e for e in exts if "atomic" in e.lower()]
            if atomics:
                print(f"    原子操作相關 extension: {', '.join(atomics)}")
            else:
                print("    無原子操作相關 extension")
        print()

def query_all_resources():
    print("[資源查詢] 開始查詢所有系統資源...")
    buf = ctypes.create_string_buffer(65536)
    svm.retryix_query_all_resources.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    svm.retryix_query_all_resources.restype = ctypes.c_int
    ret = svm.retryix_query_all_resources(buf, ctypes.sizeof(buf))
    if ret == 0:
        data = json.loads(buf.value.decode())
        print("[Python] 查詢所有資源結果：")
        print(json.dumps(data, indent=2, ensure_ascii=False))
        return data
    else:
        print(f"[Python] 查詢所有資源失敗，錯誤碼: {ret}")
        return None

class SVMManager:
    """SVM 記憶體管理器"""
    def __init__(self, context, device):
        self.context = context
        self.device = device

    def __del__(self):
        pass

    def allocate(self, size, flags):
        buf = svm.retryix_svm_alloc(self.context, size, flags)
        return buf

    def free(self, ptr):
        svm.retryix_svm_free(self.context, ptr)

    def map(self, queue, svm_ptr, size):
        event = cl.enqueue_svm_map(queue, True, cl.map_flags.READ | cl.map_flags.WRITE, svm_ptr, size)
        event.wait()
        return event

    def unmap(self, queue, svm_ptr):
        event = cl.enqueue_svm_unmap(queue, svm_ptr)
        event.wait()
        return event

if __name__ == "__main__":
    print("=== RetryIX 裝置兼容性檢查 (動態追蹤版本) ===")
    try:
        print("\n1. 執行 SVM Demo...")
        run_svm_demo()
        
        print("\n2. 查詢 OpenCL SVM 能力...")
        query_atomic_svm_capabilities()
        
        print("\n3. 執行原子操作核心 Demo...")
        run_atomic_kernel_demo()
        
        print("\n4. 查詢所有系統資源...")
        query_all_resources()
        
        print("\n=== 檢查完成 ===")
    except Exception as e:
        print(f"\n❌ 執行錯誤: {e}")
        import traceback
        traceback.print_exc()