# ===== RetryIX - Universal Makefile (auto integrate all C files) =====

# (1) Toolchain
CC      ?= gcc
AR      ?= ar
UNAME_S := $(shell uname -s 2>/dev/null)

# (2) OpenCL link flags (adjust if you have vendor SDKs)
ifeq ($(UNAME_S),Darwin)
    # macOS: OpenCL.framework
    LDFLAGS ?= -framework OpenCL
    CFLAGS  ?= -O2 -Wall -DCL_TARGET_OPENCL_VERSION=220
else
    # Linux/WSL/Unix-like:
    LDFLAGS ?= -lOpenCL
    CFLAGS  ?= -O2 -Wall -DCL_TARGET_OPENCL_VERSION=220
endif

# Windows(MinGW) 可視環境需求自行補上 OpenCL.lib 搜尋路徑，例如：
# LDFLAGS += -L"C:/Program Files (x86)/IntelSWTools/opencl/lib/x64" -lOpenCL
# CFLAGS  += -I"C:/Program Files (x86)/IntelSWTools/opencl/include"

# (3) Sources and auto integration
# 目錄內所有 .c
ALL_SRCS := $(wildcard *.c)

# 入口檔（可覆寫：make MAIN=my_cli.c）
MAIN ?= main.c

# 檢查入口檔存在
ifeq ($(filter $(MAIN),$(ALL_SRCS)),)
$(error MAIN '$(MAIN)' not found among sources: $(ALL_SRCS))
endif

# 非入口的其他 .c 會被打包成靜態庫，避免多個 main() 連結衝突
LIB_SRCS := $(filter-out $(MAIN),$(ALL_SRCS))
LIB_OBJS := $(LIB_SRCS:.c=.o)
MAIN_OBJ := $(MAIN:.c=.o)

# (4) Targets
TARGET    ?= retryix_host_demo
STATICLIB  = libretryix.a
RETRYIX_DLL = retryix.dll
RETRYIX_IMPLIB = libretryix.a
# 僅包含純 API 檔案，不含 main/cli/host
DLL_SRCS = retryix_kernel.c retryix_device_utils.c retryix_exports.c retryix_memory.c retryix_platform.c retryix_query_all_resources.c retryix_svm.c host_comm.c

.PHONY: all clean list-sources help

all: $(TARGET) retryix_cli.exe
# CLI 執行檔目標
retryix_cli.exe: retryix_cli.o $(STATICLIB)
	$(CC) -o $@ retryix_cli.o $(STATICLIB) $(LDFLAGS)

# 主程式連結：main.o + libretryix.a
$(TARGET): $(MAIN_OBJ) $(STATICLIB)
	$(CC) -o $@ $(MAIN_OBJ) $(STATICLIB) $(LDFLAGS)

# 靜態庫：打包所有非 MAIN 的 .o
$(STATICLIB): $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

$(RETRYIX_DLL): $(DLL_SRCS)
	$(CC) -O2 -Wall -shared -o $@ $^ -I. -lOpenCL -Wl,--out-implib,$(RETRYIX_IMPLIB) -DCL_TARGET_OPENCL_VERSION=200 -DCL_USE_DEPRECATED_OPENCL_1_2_APIS

# 一般編譯規則
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 便利工具：列出自動納入的來源檔
list-sources:
	@echo "MAIN     = $(MAIN)"
	@echo "LIB_SRCS = $(LIB_SRCS)"
	@echo "ALL_SRCS = $(ALL_SRCS)"

clean:
	rm -f *.o $(TARGET) $(STATICLIB) $(RETRYIX_DLL) $(RETRYIX_IMPLIB)

help:
	@echo "Usage:"
	@echo "  make                 # build with default MAIN=$(MAIN)"
	@echo "  make MAIN=my_cli.c   # choose a different entry file"
	@echo "  make list-sources    # see which files are integrated"
	@echo "  make clean"
