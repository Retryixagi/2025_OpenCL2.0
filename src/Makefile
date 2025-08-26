# ===== RetryIX OpenCL Makefile =====


CC = gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Iinclude -DCL_TARGET_OPENCL_VERSION=200
LDFLAGS = -lOpenCL
TARGET = test_platform.exe
RETRYIX_HOST = retryix_host.exe
RETRYIX_DLL = retryix.dll
RETRYIX_IMPLIB = libretryix.a
RETRYIX_SRCS = main.c retryix_kernel.c retryix_platform.c retryix_device_utils.c retryix_memory.c retryix_svm.c
HEADER = retryix.h

SRCS := $(wildcard *.c)
OBJS = main.o retryix_device_utils.o retryix_kernel.o retryix_memory.o retryix_svm.o
HOST_OBJS = retryix_host.o


# ===== Default: build all =====
all: $(TARGET) $(RETRYIX_HOST) $(RETRYIX_DLL)
$(RETRYIX_DLL): $(RETRYIX_SRCS)
	$(CC) -O2 -Wall -shared -o $@ $^ -I. -lOpenCL -Wl,--out-implib,$(RETRYIX_IMPLIB) -DCL_TARGET_OPENCL_VERSION=200 -DCL_USE_DEPRECATED_OPENCL_1_2_APIS

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(RETRYIX_HOST): $(HOST_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# ===== Clean build =====
clean:
	rm -f $(OBJS) $(TARGET) $(HOST_OBJS) $(RETRYIX_HOST)

# ===== Silent mode fix =====
.PHONY: all clean

# 工具鏈
CC      := gcc
AR      := ar
RM      := rm -f

# 旗標
CFLAGS  := -O2 -Wall -I. \
           -DCL_TARGET_OPENCL_VERSION=200 \
           -DCL_USE_DEPRECATED_OPENCL_1_2_APIS

# 連結旗標
LDFLAGS := -shared -lOpenCL -Wl,--out-implib,libretryix.a

# 輸出
DLL     := retryix.dll
IMPLIB  := libretryix.a

# 原始碼（只新增 retryix_exports.c；其他不動）
SRCS    := \
  main.c \
  retryix_kernel.c \
  retryix_platform.c \
  retryix_device_utils.c \
  retryix_memory.c \
  retryix_svm.c \
  retryix_exports.c

OBJS    := $(SRCS:.c=.o)

.PHONY: all dll clean

all: dll

dll: $(OBJS)
	$(CC) $(CFLAGS) -o $(DLL) $(OBJS) $(LDFLAGS)

# 一般 .c → .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(DLL) $(IMPLIB)