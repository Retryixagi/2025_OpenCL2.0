import pyopencl as cl
import ctypes
import json
import os
import winreg



def get_dll_path_from_registry():
    # 先查詢 VersionHistory 的 CurrentVersion
    version_key = r"SOFTWARE\\RetryIX\\VersionHistory"
    version_value = "CurrentVersion"
    current_version = None
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, version_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            current_version, _ = winreg.QueryValueEx(key, version_value)
    except Exception as e:
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, version_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
                current_version, _ = winreg.QueryValueEx(key, version_value)
        except Exception as e2:
            print("[查詢 CurrentVersion 失敗]：", e, e2)
            current_version = None

    if current_version:
        reg_path = fr"SOFTWARE\\RetryIX\\MemoryRAID\\v{current_version}"
        value_name = "DllPath"
        # 先嘗試 64 位元 key
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, reg_path, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
                dll_path, _ = winreg.QueryValueEx(key, value_name)
                return dll_path
        except Exception as e:
            print(f"[64位元註冊表查詢失敗]：{reg_path}", e)
        # 再嘗試 32 位元 key
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, reg_path, 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
                dll_path, _ = winreg.QueryValueEx(key, value_name)
                return dll_path
        except Exception as e:
            print(f"[32位元註冊表查詢失敗]：{reg_path}", e)
    return None

class RetryIXSVMContext(ctypes.Structure):
    pass

class RetryIXDLL:
    def __init__(self, dll_path=None):
        if dll_path is None:
            dll_path = get_dll_path_from_registry()
            if dll_path is None:
                raise RuntimeError("找不到 DLL 路徑，請確認註冊表已正確設置 DllPath！")
        self.dll_path = dll_path
        print("載入 DLL 路徑：", self.dll_path)
        print("檔案大小：", os.path.getsize(self.dll_path))
        self.svm = ctypes.CDLL(self.dll_path)
        print("retryix_query_all_resources in dir(svm):", "retryix_query_all_resources" in dir(self.svm))
        self._setup_api()

    def _setup_api(self):
        self.svm.retryix_svm_create_context.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
        self.svm.retryix_svm_create_context.restype = ctypes.POINTER(RetryIXSVMContext)
        self.svm.retryix_svm_destroy_context.argtypes = [ctypes.POINTER(RetryIXSVMContext)]
        self.svm.retryix_svm_destroy_context.restype = None
        self.svm.retryix_svm_alloc.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_size_t, ctypes.c_int]
        self.svm.retryix_svm_alloc.restype = ctypes.c_void_p
        self.svm.retryix_svm_free.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_void_p]
        self.svm.retryix_svm_free.restype = ctypes.c_int
        self.svm.retryix_kernel_atomic_add_demo.argtypes = []
        self.svm.retryix_kernel_atomic_add_demo.restype = None
        self.svm.retryix_query_all_resources.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
        self.svm.retryix_query_all_resources.restype = ctypes.c_int

    def run_atomic_kernel_demo(self):
        print("[Python] 呼叫 retryix_kernel_atomic_add_demo() ...")
        self.svm.retryix_kernel_atomic_add_demo()
        print("[Python] atomic_add demo 執行完畢。")

    def run_svm_demo(self):
        platform = cl.get_platforms()[0]
        device = platform.get_devices()[0]
        ctx = cl.Context([device])
        queue = cl.CommandQueue(ctx)
        cl_context_ptr = ctypes.c_void_p(ctx.int_ptr)
        cl_device_id_ptr = ctypes.c_void_p(device.int_ptr)
        svm_ctx = self.svm.retryix_svm_create_context(cl_context_ptr, cl_device_id_ptr)
        RETRYIX_SVM_FINE_GRAIN = 2
        RETRYIX_SVM_ATOMIC = 0x8
        buf = self.svm.retryix_svm_alloc(svm_ctx, 4096, RETRYIX_SVM_FINE_GRAIN | RETRYIX_SVM_ATOMIC)
        print(f"SVM buffer allocated at: {buf}")
        self.svm.retryix_svm_free(svm_ctx, buf)
        print("SVM buffer freed.")
        self.svm.retryix_svm_destroy_context(svm_ctx)
        print("SVM context destroyed.")

    def query_atomic_svm_capabilities(self):
        platforms = cl.get_platforms()
        for p in platforms:
            print(f"平台: {p.name}")
            for d in p.get_devices():
                print(f"  裝置: {d.name}")
                exts = d.extensions.split()
                print(f"    Extensions: {' '.join(exts)}")
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
                    print("    (無法查詢 SVM 能力，可能 OpenCL <2.0)")
                atomics = [e for e in exts if "atomic" in e.lower()]
                if atomics:
                    print(f"    原子操作相關 extension: {', '.join(atomics)}")
                else:
                    print("    無原子操作相關 extension")
            print()

    def query_all_resources(self, buf_size=65536):
        buf = ctypes.create_string_buffer(buf_size)
        ret = self.svm.retryix_query_all_resources(buf, buf_size)
        if ret == 0:
            data = json.loads(buf.value.decode())
            print("[Python] 查詢所有資源結果：")
            print(json.dumps(data, indent=2, ensure_ascii=False))
            return data
        else:
            print("[Python] 查詢所有資源失敗")
            return None

class SVMManager:
    def __init__(self, context, device, dll: RetryIXDLL):
        self.context = context
        self.device = device
        self.dll = dll

    def __del__(self):
        pass

    def allocate(self, size, flags):
        buf = self.dll.svm.retryix_svm_alloc(self.context, size, flags)
        return buf

    def free(self, ptr):
        self.dll.svm.retryix_svm_free(self.context, ptr)

    def map(self, queue, svm_ptr, size):
        event = cl.enqueue_svm_map(queue, True, cl.map_flags.READ | cl.map_flags.WRITE, svm_ptr, size)
        event.wait()
        return event

    def unmap(self, queue, svm_ptr):
        event = cl.enqueue_svm_unmap(queue, svm_ptr)
        event.wait()
        return event

if __name__ == "__main__":
    dll = RetryIXDLL()  # 可自訂路徑：RetryIXDLL("C:/your/path/retryix.dll")
    dll.run_svm_demo()
    dll.query_atomic_svm_capabilities()
    dll.run_atomic_kernel_demo()
    dll.query_all_resources()
