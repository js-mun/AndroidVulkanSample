#pragma once

static const char* kTAG = "MyVulkan";

#ifndef DEBUG_LOG_VERBOSE
#define DEBUG_LOG_VERBOSE 0
#endif

#if defined(__ANDROID__)

#include <android/log.h>

#if DEBUG_LOG_VERBOSE
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, kTAG, __VA_ARGS__))
#else
#define LOGV(...) ((void)0)
#endif
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, kTAG, __VA_ARGS__))
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

#else

#include <chrono>
#include <cstdio>
#include <ctime>

namespace log_internal {
inline const char* nowTimestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    const std::time_t tt = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&tt, &tm);

    thread_local char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
    return buffer;
}
} // namespace log_internal

#if DEBUG_LOG_VERBOSE
#define LOGV(...) do { std::fprintf(stdout, "[%s][V] %s: ", log_internal::nowTimestamp(), kTAG); std::fprintf(stdout, __VA_ARGS__); std::fprintf(stdout, "\n"); } while (0)
#else
#define LOGV(...) ((void)0)
#endif
#define LOGD(...) do { std::fprintf(stdout, "[%s][D] %s: ", log_internal::nowTimestamp(), kTAG); std::fprintf(stdout, __VA_ARGS__); std::fprintf(stdout, "\n"); } while (0)
#define LOGI(...) do { std::fprintf(stdout, "[%s][I] %s: ", log_internal::nowTimestamp(), kTAG); std::fprintf(stdout, __VA_ARGS__); std::fprintf(stdout, "\n"); } while (0)
#define LOGW(...) do { std::fprintf(stderr, "[%s][W] %s: ", log_internal::nowTimestamp(), kTAG); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)
#define LOGE(...) do { std::fprintf(stderr, "[%s][E] %s: ", log_internal::nowTimestamp(), kTAG); std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); } while (0)

#endif
