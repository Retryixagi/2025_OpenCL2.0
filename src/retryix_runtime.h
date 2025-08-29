// retryix_runtime.h
int retryix_runtime_load_kernel(const char* filepath);
int retryix_runtime_run_kernel(const char* kernel_name, void** args, size_t arg_count);
int retryix_runtime_wait();

int retryix_runtime_query_metrics(char* buffer, size_t max_len);
int retryix_runtime_shutdown();