import pyopencl as cl
import ctypes
import json
import os
import winreg

def get_dll_path():
    """Dynamically locate RetryIX DLL path"""
    print("[Dynamic Tracking] Starting DLL path search...")
    
    # Method 1: Query Windows Registry
    try:
        version_key = r"SOFTWARE\\RetryIX\\VersionHistory"
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, version_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            current_version, _ = winreg.QueryValueEx(key, "CurrentVersion")
            print(f"[Dynamic Tracking] Found version: {current_version}")
            
            # Query actual DLL path
            dll_key = fr"SOFTWARE\\RetryIX\\MemoryRAID\\v{current_version}"
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, dll_key, 0, winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as dll_reg:
                dll_path, _ = winreg.QueryValueEx(dll_reg, "DllPath")
                if os.path.exists(dll_path):
                    print(f"[Dynamic Tracking] Registry path valid: {dll_path}")
                    return dll_path
                else:
                    print(f"[Dynamic Tracking] Registry path invalid: {dll_path}")
    except Exception as e:
        print(f"[Dynamic Tracking] Registry query failed: {e}")
    
    # Method 2: Common installation paths
    common_paths = [
        r"C:\Program Files\RetryIX\bin\retryix.dll",
        r"C:\Program Files (x86)\RetryIX\bin\retryix.dll",
        r"C:\RetryIX\bin\retryix.dll",
        r"F:\opencl\src\retryix.dll",  # Original path
    ]
    
    for path in common_paths:
        if os.path.exists(path):
            print(f"[Dynamic Tracking] Found in common path: {path}")
            return path
    
    # Method 3: Search in script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    local_dll = os.path.join(script_dir, "retryix.dll")
    if os.path.exists(local_dll):
        print(f"[Dynamic Tracking] Found in script directory: {local_dll}")
        return local_dll
    
    # Method 4: Search in current working directory
    cwd_dll = os.path.join(os.getcwd(), "retryix.dll")
    if os.path.exists(cwd_dll):
        print(f"[Dynamic Tracking] Found in working directory: {cwd_dll}")
        return cwd_dll
    
    raise FileNotFoundError("Cannot find retryix.dll! Please ensure RetryIX is properly installed")

# Dynamically load DLL
dll_path = get_dll_path()
print("Loading DLL path:", dll_path)
print("File size:", os.path.getsize(dll_path))
svm = ctypes.CDLL(dll_path)
print("retryix_query_all_resources in dir(svm):", "retryix_query_all_resources" in dir(svm))

class RetryIXSVMContext(ctypes.Structure):
    pass

# Define API signatures
svm.retryix_svm_create_context.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
svm.retryix_svm_create_context.restype = ctypes.POINTER(RetryIXSVMContext)
svm.retryix_svm_destroy_context.argtypes = [ctypes.POINTER(RetryIXSVMContext)]
svm.retryix_svm_destroy_context.restype = None
svm.retryix_svm_alloc.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_size_t, ctypes.c_int]
svm.retryix_svm_alloc.restype = ctypes.c_void_p
svm.retryix_svm_free.argtypes = [ctypes.POINTER(RetryIXSVMContext), ctypes.c_void_p]
svm.retryix_svm_free.restype = ctypes.c_int

# Add: Atomic kernel test API
svm.retryix_kernel_atomic_add_demo.argtypes = []
svm.retryix_kernel_atomic_add_demo.restype = None

def run_atomic_kernel_demo():
    print("[Python] Calling retryix_kernel_atomic_add_demo()...")
    svm.retryix_kernel_atomic_add_demo()
    print("[Python] Atomic add demo completed.")

def run_svm_demo():
    print("[SVM Demo] Starting...")
    # Create OpenCL context/device/queue using pyopencl
    platform = cl.get_platforms()[0]
    device = platform.get_devices()[0]
    print(f"[SVM Demo] Using platform: {platform.name}")
    print(f"[SVM Demo] Using device: {device.name}")
    
    ctx = cl.Context([device])
    queue = cl.CommandQueue(ctx)
    cl_context_ptr = ctypes.c_void_p(ctx.int_ptr)
    cl_device_id_ptr = ctypes.c_void_p(device.int_ptr)

    # Create SVM context
    print("[SVM Demo] Creating SVM context...")
    svm_ctx = svm.retryix_svm_create_context(cl_context_ptr, cl_device_id_ptr)

    # Allocate SVM memory
    print("[SVM Demo] Allocating SVM memory...")
    RETRYIX_SVM_FINE_GRAIN = 2
    RETRYIX_SVM_ATOMIC = 0x8
    buf = svm.retryix_svm_alloc(svm_ctx, 4096, RETRYIX_SVM_FINE_GRAIN | RETRYIX_SVM_ATOMIC)
    print(f"SVM buffer allocated at: 0x{buf:x}")

    # Free SVM memory
    print("[SVM Demo] Freeing SVM memory...")
    svm.retryix_svm_free(svm_ctx, buf)
    print("SVM buffer freed.")

    # Destroy SVM context
    print("[SVM Demo] Destroying SVM context...")
    svm.retryix_svm_destroy_context(svm_ctx)
    print("SVM context destroyed.")

def query_atomic_svm_capabilities():
    print("[Capability Query] Querying OpenCL SVM and atomic operation capabilities...")
    platforms = cl.get_platforms()
    for p in platforms:
        print(f"Platform: {p.name}")
        for d in p.get_devices():
            print(f"  Device: {d.name}")
            exts = d.extensions.split()
            print(f"    Extensions count: {len(exts)}")
            
            try:
                svm_caps = d.get_info(cl.device_info.SVM_CAPABILITIES)
                print(f"    SVM Capabilities: 0x{svm_caps:x}")
                if svm_caps & cl.device_svm_capabilities.FINE_GRAIN_BUFFER:
                    print("      ✓ Supports FINE_GRAIN_BUFFER")
                if svm_caps & cl.device_svm_capabilities.FINE_GRAIN_SYSTEM:
                    print("      ✓ Supports FINE_GRAIN_SYSTEM")
                if svm_caps & cl.device_svm_capabilities.ATOMICS:
                    print("      ✓ Supports ATOMICS")
                if svm_caps & cl.device_svm_capabilities.COARSE_GRAIN_BUFFER:
                    print("      ✓ Supports COARSE_GRAIN_BUFFER")
            except Exception as e:
                print(f"    (Cannot query SVM capabilities: {e})")
                
            atomics = [e for e in exts if "atomic" in e.lower()]
            if atomics:
                print(f"    Atomic operation extensions: {', '.join(atomics)}")
            else:
                print("    No atomic operation extensions found")
        print()

def query_all_resources():
    print("[Resource Query] Querying all system resources...")
    buf = ctypes.create_string_buffer(65536)
    svm.retryix_query_all_resources.argtypes = [ctypes.c_char_p, ctypes.c_size_t]
    svm.retryix_query_all_resources.restype = ctypes.c_int
    ret = svm.retryix_query_all_resources(buf, ctypes.sizeof(buf))
    if ret == 0:
        data = json.loads(buf.value.decode())
        print("[Python] All resources query result:")
        print(json.dumps(data, indent=2, ensure_ascii=False))
        return data
    else:
        print(f"[Python] All resources query failed, error code: {ret}")
        return None

class SVMManager:
    """SVM Memory Manager"""
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
    print("=== RetryIX Device Compatibility Check (Dynamic Tracking Version) ===")
    try:
        print("\n1. Running SVM Demo...")
        run_svm_demo()
        
        print("\n2. Querying OpenCL SVM capabilities...")
        query_atomic_svm_capabilities()
        
        print("\n3. Running atomic operation kernel demo...")
        run_atomic_kernel_demo()
        
        print("\n4. Querying all system resources...")
        query_all_resources()
        
        print("\n=== Check completed ===")
    except Exception as e:
        print(f"\n✗ Execution error: {e}")
        import traceback
        traceback.print_exc()