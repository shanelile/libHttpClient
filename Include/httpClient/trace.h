// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once


// These defines are not meant to be used by clients at runtime, the
// HCTraceLevel enum should be used instead
#define HC_PRIVATE_TRACE_LEVEL_OFF 0
#define HC_PRIVATE_TRACE_LEVEL_ERROR 1
#define HC_PRIVATE_TRACE_LEVEL_WARNING 2
#define HC_PRIVATE_TRACE_LEVEL_IMPORTANT 3
#define HC_PRIVATE_TRACE_LEVEL_INFORMATION 4
#define HC_PRIVATE_TRACE_LEVEL_VERBOSE 5

//------------------------------------------------------------------------------
// Trace level enum
//------------------------------------------------------------------------------
typedef enum HCTraceLevel
{
    HCTraceLevel_Off = HC_PRIVATE_TRACE_LEVEL_OFF,
    HCTraceLevel_Error = HC_PRIVATE_TRACE_LEVEL_ERROR,
    HCTraceLevel_Warning = HC_PRIVATE_TRACE_LEVEL_WARNING,
    HCTraceLevel_Important = HC_PRIVATE_TRACE_LEVEL_IMPORTANT,
    HCTraceLevel_Information = HC_PRIVATE_TRACE_LEVEL_INFORMATION,
    HCTraceLevel_Verbose = HC_PRIVATE_TRACE_LEVEL_VERBOSE,
} HCTraceLevel;

//------------------------------------------------------------------------------
// Configuration
//------------------------------------------------------------------------------
// All macros used for configuration are always defined. Clients can define
// their own values for these macros to control tracing behavior
//
// HC_TRACE_BUILD_LEVEL [trace level (0-5)]
//     controls the maximum level of verbosity that will be built in the
//     executable. To control verbosity at runtime see TraceArea. Set to 0 to 
//     completely disable tracing
//
// HC_TRACE_TO_DEBUGGER [0,1]
//     controls if trace will output using OutputDebugString
//     only has an effect when building trace.cpp
//
// HC_TRACE_TO_CLIENT [0,1]
//     controls if trace will output using the TraceLoggingProvider
//     only has an effect when building trace.cpp

#ifndef HC_TRACE_BUILD_LEVEL
#define HC_TRACE_BUILD_LEVEL HC_PRIVATE_TRACE_LEVEL_VERBOSE
#endif

#ifndef HC_TRACE_TO_DEBUGGER
#define HC_TRACE_TO_DEBUGGER 1
#endif

#ifndef HC_TRACE_TO_CLIENT
#define HC_TRACE_TO_CLIENT 1
#endif

//------------------------------------------------------------------------------
// Level enabled macros
//------------------------------------------------------------------------------
// These macros can be used to easily check if a given trace level will be
// included in the build (for example to avoid compiling helper code for a
// trace).
// These macros are always defined
//
// HC_TRACE_ENABLE [0,1]
// HC_TRACE_ERROR_ENABLE [0,1]
// HC_TRACE_WARNING_ENABLE [0,1]
// HC_TRACE_IMPORTANT_ENABLE [0,1]
// HC_TRACE_INFORMATION_ENABLE [0,1]
// HC_TRACE_VERBOSE_ENABLE [0,1]
//
// these should not be manually modified, they are automatically set based on
// the values of HC_TRACE_BUILD_LEVEL

#if HC_TRACE_BUILD_LEVEL > HC_PRIVATE_TRACE_LEVEL_OFF
#define HC_TRACE_ENABLE 1
#else
#define HC_TRACE_ENABLE 0
#endif

#if HC_TRACE_BUILD_LEVEL >= HC_PRIVATE_TRACE_LEVEL_ERROR
#define HC_TRACE_ERROR_ENABLE 1
#else
#define HC_TRACE_ERROR_ENABLE 0
#endif


#if HC_TRACE_BUILD_LEVEL >= HC_PRIVATE_TRACE_LEVEL_WARNING
#define HC_TRACE_WARNING_ENABLE 1
#else
#define HC_TRACE_WARNING_ENABLE 0
#endif


#if HC_TRACE_BUILD_LEVEL >= HC_PRIVATE_TRACE_LEVEL_IMPORTANT
#define HC_TRACE_IMPORTANT_ENABLE 1
#else
#define HC_TRACE_IMPORTANT_ENABLE 0
#endif


#if HC_TRACE_BUILD_LEVEL >= HC_PRIVATE_TRACE_LEVEL_INFORMATION
#define HC_TRACE_INFORMATION_ENABLE 1
#else
#define HC_TRACE_INFORMATION_ENABLE 0
#endif


#if HC_TRACE_BUILD_LEVEL >= HC_PRIVATE_TRACE_LEVEL_VERBOSE
#define HC_TRACE_VERBOSE_ENABLE 1
#else
#define HC_TRACE_VERBOSE_ENABLE 0
#endif

//------------------------------------------------------------------------------
// Client callback
//------------------------------------------------------------------------------
// Register callback for tracing so that the client can merge tracing into their
// own logs
// These macros are always defined but will compile to nothing if trace is
// disabled

typedef void (HCTraceCallback)(
    char const* areaName,
    enum HCTraceLevel level,
    unsigned int threadId,
    uint64_t timestamp,
    char const* message
);

STDAPI_(void) HCTraceSetClientCallback(HCTraceCallback* callback);

//------------------------------------------------------------------------------
// Trace area macros
//------------------------------------------------------------------------------
// These macros are used to set up areas for tracing
// These macros are always defined but will compile to nothing if trace is
// disabled

#if HC_TRACE_ENABLE
// Defines an area for tracing:
// name is used as a prefix for all traces in the area
// verbosity sets the maximum verbosity that will be traced within this area
// verbosity can be tweaked at runtime by calling HC_TRACE_SET_VERBOSITY(area,
// level)
// since this defines a global variable, it should be only used from a .cpp file
// and each area should be defined only once
#define HC_DEFINE_TRACE_AREA(area, verbosity) \
    struct HCTraceImplArea HC_PRIVATE_TRACE_AREA_NAME(area) = { #area, (verbosity) }

// Declares a trace area. Since DEFINE_TRACE_AREA can only be used once and in a
// .cpp, this allows to trace in an already defined area from another file
#define HC_DECLARE_TRACE_AREA(area) \
    extern struct HCTraceImplArea HC_PRIVATE_TRACE_AREA_NAME(area)

// Access to the verbosity property of the area for runtime control
#define HC_TRACE_SET_VERBOSITY(area, level) \
    HCTraceImplSetAreaVerbosity(&HC_PRIVATE_TRACE_AREA_NAME(area), level)

#define HC_TRACE_GET_VERBOSITY(area) \
    HCTraceImplGetAreaVerbosity(&HC_PRIVATE_TRACE_AREA_NAME(area))
#else
#define HC_DEFINE_TRACE_AREA(name, verbosity)
#define HC_DECLARE_TRACE_AREA(name)
#define HC_TRACE_SET_VERBOSITY(area, level)
#define HC_TRACE_GET_VERBOSITY(area) HCTraceLevel_Off
#endif

//------------------------------------------------------------------------------
// Trace macros
//------------------------------------------------------------------------------
// These are the macros to be used to log
// These macros are always defined but will compile to nothing if
// HC_TRACE_BUILD_LEVEL is not high enough

#if HC_TRACE_ENABLE
#define HC_TRACE_MESSAGE(area, level, format, ...) \
    HCTraceImplMessage(&HC_PRIVATE_TRACE_AREA_NAME(area), (level), format, ##__VA_ARGS__)

#ifdef __cplusplus
#define HC_TRACE_SCOPE(area, level) \
        auto tsh = HCTraceImplScopeHelper{ &HC_PRIVATE_TRACE_AREA_NAME(area), level, HC_FUNCTION }
#else
#define HC_TRACE_SCOPE(area, level)
#endif
#else
#define HC_TRACE_MESSAGE(area, level, ...)
#define HC_TRACE_MESSAGE_WITH_LOCATION(area, level, format, ...)
#define HC_TRACE_SCOPE(area, level)
#endif

#if HC_TRACE_ERROR_ENABLE
#define HC_TRACE_ERROR(area, msg, ...)  \
    HC_TRACE_MESSAGE(area, HCTraceLevel_Error, msg, ##__VA_ARGS__)

#define HC_TRACE_ERROR_HR(area, failedHr, msg)  \
    HC_TRACE_ERROR(area, "%hs (hr=0x%08x)", msg, failedHr)
#else
#define HC_TRACE_ERROR(area, msg, ...)
#define HC_TRACE_ERROR_HR(area, failedHr, msg)
#endif

#if HC_TRACE_WARNING_ENABLE
#define HC_TRACE_WARNING(area, msg, ...) \
    HC_TRACE_MESSAGE(area, HCTraceLevel_Warning, msg, ##__VA_ARGS__)

#define HC_TRACE_WARNING_HR(area, failedHr, msg)  \
    HC_TRACE_WARNING(area, "%hs (hr=0x%08x)", msg, failedHr)
#else
#define HC_TRACE_WARNING(area, msg, ...)
#define HC_TRACE_WARNING_HR(area, failedHr, msg)
#endif

#if HC_TRACE_IMPORTANT_ENABLE
#define HC_TRACE_IMPORTANT(area, msg, ...) \
    HC_TRACE_MESSAGE(area, HCTraceLevel_Important, msg, ##__VA_ARGS__)

#define HC_TRACE_SCOPE_IMPORTANT(area) \
    HC_TRACE_SCOPE(area, HCTraceLevel_Important)
#else
#define HC_TRACE_IMPORTANT(area, msg, ...)
#define HC_TRACE_SCOPE_IMPORTANT(area)
#endif

#if HC_TRACE_INFORMATION_ENABLE
#define HC_TRACE_INFORMATION(area, msg, ...) \
    HC_TRACE_MESSAGE(area, HCTraceLevel_Information, msg, ##__VA_ARGS__)

#define HC_TRACE_SCOPE_INFORMATION(area) \
    HC_TRACE_SCOPE(area, HCTraceLevel_Information)
#else
#define HC_TRACE_INFORMATION(area, msg, ...)
#define HC_TRACE_SCOPE_INFORMATION(area)
#endif

#if HC_TRACE_VERBOSE_ENABLE
#define HC_TRACE_VERBOSE(area, msg, ...) \
        HC_TRACE_MESSAGE(area, HCTraceLevel_Verbose, msg, ##__VA_ARGS__)

#define HC_TRACE_SCOPE_VERBOSE(area) \
    HC_TRACE_SCOPE(area, HCTraceLevel_Verbose)
#else
#define HC_TRACE_VERBOSE(area, msg, ...)
#define HC_TRACE_SCOPE_VERBOSE(area)
#endif

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
// These symbols are always built even if trace is disabled, but note that no
// trace areas will be defined so they should never be used directly
// the reason why they are always built is to avoid issues with different
// compilation units setting different values for HC_TRACE_BUILD_LEVEL
// the linker will discard everything if tracing is disabled because there will
// be no references

// DO NOT USE THESE SYMBOLS DIRECTLY

#define HC_PRIVATE_TRACE_AREA_NAME(area) g_trace##area
#define HC_FUNCTION __FUNCTION__

typedef struct HCTraceImplArea
{
    char const* const Name;
    enum HCTraceLevel Verbosity;
} HCTraceImplArea;

inline
void STDAPIVCALLTYPE HCTraceImplSetAreaVerbosity(struct HCTraceImplArea* area, enum HCTraceLevel verbosity)
{
    area->Verbosity = verbosity;
}

inline
enum HCTraceLevel HCTraceImplGetAreaVerbosity(struct HCTraceImplArea* area)
{
    return area->Verbosity;
}

STDAPI_(void) HCTraceImplMessage(
    struct HCTraceImplArea const* area,
    enum HCTraceLevel level,
    _Printf_format_string_ char const* format,
    ...
    );


#if defined(__cplusplus)
class HCTraceImplScopeHelper
{
public:
    HCTraceImplScopeHelper(HCTraceImplArea const* area, HCTraceLevel level, char const* scope);
    ~HCTraceImplScopeHelper();

private:
    HCTraceImplArea const* m_area;
    HCTraceLevel const m_level;
    char const* const m_scope;
    unsigned long long const m_id;
};
#endif // defined(__cplusplus)
