#include "pch.h"
#include <unistd.h>
#include <Android/log.h>

void OutputDebugString(char const* string) {
    __android_log_print(ANDROID_LOG_DEFAULT, "libHttpClient", "%s", string);
}

unsigned int GetThreadId() {
    return gettid();
}

unsigned long long GetScopeId() {
    // TODO: 
    return 0;
}