#ifndef KBO_CORE_LOG_H
#define KBO_CORE_LOG_H

void kbo_log_runtime_line_at(const char* file, int line, const char* message);
void kbo_log_runtimef_at(const char* file, int line, const char* format, ...);

#define kbo_log_runtime_line(message) kbo_log_runtime_line_at(__FILE__, __LINE__, (message))
#define kbo_log_runtimef(...) kbo_log_runtimef_at(__FILE__, __LINE__, __VA_ARGS__)

#endif
