#pragma once

#include <httpClient/trace.h>

void HCTraceImplGlobalInit();
void HCTraceImplGlobalCleanup();

void OutputDebugString(char const* string);

unsigned int GetThreadId();

unsigned long long GetScopeId();