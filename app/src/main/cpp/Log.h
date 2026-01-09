//
// Created by mj on 1/9/26.
//

#ifndef MYGAME_LOG_H
#define MYGAME_LOG_H

#include <android/log.h>

static const char* kTAG = "MyVulkan";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))



#endif //MYGAME_LOG_H
