import ctypes
from ctypes import Structure, c_char, c_ulong, c_ulonglong, c_int, c_char_p, POINTER, byref, create_string_buffer
import os
print(os.getcwd())
retryix = ctypes.cdll.LoadLibrary(r"F:\opencl\src\retryix.dll")

# Struct 定義（對應 C 的 RetryIXDeviceInfo 結構）
class RetryIXDeviceInfo(Structure):
    _fields_ = [
        ("name", c_char * 256),
        ("type", c_ulong),
        ("global_mem_size", c_ulonglong),
        ("opencl_version", c_char * 128)
    ]

# 函式定義
retryix.retryix_enumerate_devices.restype = c_int
retryix.retryix_get_device_info.argtypes = [c_int, POINTER(RetryIXDeviceInfo)]
retryix.retryix_get_device_info.restype = c_int
retryix.retryix_export_device_info_json.argtypes = [c_int, c_char_p, c_int]
retryix.retryix_export_device_info_json.restype = c_int
retryix.retryix_enumerate_platforms.argtypes = [c_char_p, c_int]
retryix.retryix_enumerate_platforms.restype = c_int
retryix.retryix_export_all_devices_json.argtypes = [c_char_p]
retryix.retryix_export_all_devices_json.restype = c_int

# 執行查詢
def run_device_query():
    count = retryix.retryix_enumerate_devices()
    print(f"\n🔍 Found {count} OpenCL device(s):\n")

    for i in range(count):
        info = RetryIXDeviceInfo()
        result = retryix.retryix_get_device_info(i, byref(info))
        if result == 0:
            print(f"[{i}] {info.name.decode()}")
            print(f"    Type: {info.type}")
            print(f"    Mem: {info.global_mem_size / (1024*1024):.2f} MB")
            print(f"    Version: {info.opencl_version.decode()}")
        else:
            print(f"[{i}] Failed to get info")

def run_json_query(index=0):
    json_buf = create_string_buffer(1024)
    result = retryix.retryix_export_device_info_json(index, json_buf, 1024)
    if result == 0:
        print(f"\n📦 JSON for device[{index}]:\n{json_buf.value.decode()}")
    else:
        print("❌ Failed to export device info to JSON")

def run_platform_list():
    buf = create_string_buffer(1024)
    result = retryix.retryix_enumerate_platforms(buf, 1024)
    if result > 0:
        print(f"\n🧩 Detected {result} platform(s):\n{buf.value.decode()}")
    else:
        print("❌ Failed to enumerate platforms")

def export_all_devices_json(path=r"F:\opencl\device_report.json"):
    result = retryix.retryix_export_all_devices_json(path.encode())
    if result == 0:
        print(f"✅ 已成功導出所有裝置資訊到 {path}")
    else:
        print(f"❌ 導出失敗，錯誤碼: {result}")

if __name__ == "__main__":
    run_platform_list()
    run_device_query()
    run_json_query(0)
    export_all_devices_json()