import pyopencl as cl
import ctypes
import json
import os, ctypes
dll_path = os.path.abspath(r"F:\opencl\src\retryix.dll")
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
    # pyopencl 建立 context/device/queue
    platform = cl.get_platforms()[0]
    device = platform.get_devices()[0]
    ctx = cl.Context([device])
    queue = cl.CommandQueue(ctx)
    cl_context_ptr = ctypes.c_void_p(ctx.int_ptr)
    cl_device_id_ptr = ctypes.c_void_p(device.int_ptr)

    # 建立 SVM context
    svm_ctx = svm.retryix_svm_create_context(cl_context_ptr, cl_device_id_ptr)

    # 分配 SVM 記憶體
    RETRYIX_SVM_FINE_GRAIN = 2
    RETRYIX_SVM_ATOMIC = 0x8
    buf = svm.retryix_svm_alloc(svm_ctx, 4096, RETRYIX_SVM_FINE_GRAIN | RETRYIX_SVM_ATOMIC)
    print(f"SVM buffer allocated at: {buf}")

    # ...可進行資料操作、傳遞給 kernel ...

    # 釋放 SVM 記憶體
    svm.retryix_svm_free(svm_ctx, buf)
    print("SVM buffer freed.")

    # 銷毀 SVM context
    svm.retryix_svm_destroy_context(svm_ctx)
    print("SVM context destroyed.")

def query_atomic_svm_capabilities():
    import pyopencl as cl
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

def query_all_resources():
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
        print("[Python] 查詢所有資源失敗")
        return None

if __name__ == "__main__":
    run_svm_demo()
    query_atomic_svm_capabilities()
    run_atomic_kernel_demo()
    query_all_resources()

class SVMManager:
    def __init__(self, context, device):
        self.context = context
        self.device = device

    def __del__(self):
        pass

    def allocate(self, size, flags):
        import pyopencl as cl
        buf = svm.retryix_svm_alloc(self.context, size, flags)
        return buf

    def free(self, ptr):
        import pyopencl as cl
        svm.retryix_svm_free(self.context, ptr)

    def map(self, queue, svm_ptr, size):
        import pyopencl as cl
        event = cl.enqueue_svm_map(queue, True, cl.map_flags.READ | cl.map_flags.WRITE, svm_ptr, size)
        event.wait()
        return event

    def unmap(self, queue, svm_ptr):
        import pyopencl as cl
        event = cl.enqueue_svm_unmap(queue, svm_ptr)
        event.wait()
        return event