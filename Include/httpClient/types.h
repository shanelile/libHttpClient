// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once
#pragma warning(disable: 4062)
#pragma warning(disable: 4702)
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>

    #ifndef _WIN32_WINNT_WIN10
    #define _WIN32_WINNT_WIN10 0x0A00
    #endif

    #ifndef HC_XDK_API
    #define HC_XDK_API (WINAPI_FAMILY == WINAPI_FAMILY_TV_APP || WINAPI_FAMILY == WINAPI_FAMILY_TV_TITLE) 
    #endif

    #ifndef HC_UWP_API
    #define HC_UWP_API (WINAPI_FAMILY == WINAPI_FAMILY_APP && _WIN32_WINNT >= _WIN32_WINNT_WIN10)
    #endif

#endif //#ifdef _WIN32

#if HC_UNITTEST_API
#undef HC_UWP_API
#define HC_UWP_API 1
#endif 

#ifndef _WIN32
    #ifdef _In_
    #undef _In_
    #endif
    #define _In_

    #ifdef _Ret_maybenull_
    #undef _Ret_maybenull_
    #endif
    #define _Ret_maybenull_

    #ifdef _Post_writable_byte_size_
    #undef _Post_writable_byte_size_
    #endif
    #define _Post_writable_byte_size_(X)
#endif

#if defined _WIN32
  #ifdef HC_API_NONE
    #define HC_API
    #if _MSC_VER >= 1900
        #define HC_API_DEPRECATED __declspec(deprecated)
    #else
        #define HC_API_DEPRECATED
    #endif
  #else
    #ifdef HC_API_EXPORT
      #define HC_API __declspec(dllexport)
      #define HC_API_DEPRECATED __declspec(dllexport, deprecated)
    #else
      #define HC_API __declspec(dllimport)
      #define HC_API_DEPRECATED __declspec(dllimport, deprecated)
    #endif
  #endif
#else
  #if defined _NOHC_API || __GNUC__ < 4
     #define HC_API
     #define HC_API_DEPRECATED __attribute__ ((deprecated))
  #else
    #define HC_API __attribute__ ((visibility ("default")))
    #define HC_API_DEPRECATED __attribute__ ((visibility ("default"), deprecated))
  #endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define HC_CALLING_CONV __cdecl
typedef uint32_t HC_MEMORY_TYPE;
typedef struct HC_WEBSOCKET* HC_WEBSOCKET_HANDLE;
typedef struct HC_CALL* HC_CALL_HANDLE;
typedef struct HC_CALL* HC_MOCK_CALL_HANDLE;
typedef uint64_t HC_TASK_HANDLE;
typedef uint64_t HC_TASK_EVENT_HANDLE;

typedef enum HC_RESULT
{
    HC_OK = 0,
    HC_E_FAIL = -1,
    HC_E_POINTER = -2,
    HC_E_INVALIDARG = -3,
    HC_E_OUTOFMEMORY = -4,
    HC_E_BUFFERTOOSMALL = -5,
    HC_E_NOTINITIALISED = -6,
    HC_E_FEATURENOTPRESENT = -7,
    HC_E_PERFORMALREADYCALLED = -8,
    HC_E_ALREADYINITIALISED = -9,
    HC_E_CONNECTALREADYCALLED = -10,
} HC_RESULT;

HRESULT HCtoHRESULT(_In_ HC_RESULT hc);
HC_RESULT HRESULTtoHC(_In_ HRESULT hr);

// Error codes from https://www.iana.org/assignments/websocket/websocket.xml#close-code-number
typedef enum HC_WEBSOCKET_CLOSE_STATUS
{
    HC_WEBSOCKET_CLOSE_NORMAL = 1000,
    HC_WEBSOCKET_CLOSE_GOING_AWAY = 1001,
    HC_WEBSOCKET_CLOSE_PROTOCOL_ERROR = 1002,
    HC_WEBSOCKET_CLOSE_UNSUPPORTED = 1003, 
    HC_WEBSOCKET_CLOSE_ABNORMAL_CLOSE = 1006,
    HC_WEBSOCKET_CLOSE_INCONSISTENT_DATATYPE = 1007,
    HC_WEBSOCKET_CLOSE_POLICY_VIOLATION = 1008,
    HC_WEBSOCKET_CLOSE_TOO_LARGE = 1009,
    HC_WEBSOCKET_CLOSE_NEGOTIATE_ERROR = 1010,
    HC_WEBSOCKET_CLOSE_SERVER_TERMINATE = 1011,
    HC_WEBSOCKET_CLOSE_UNKNOWN_ERROR = 4000
} HC_WEBSOCKET_CLOSE_STATUS;

typedef enum HC_SUBSYSTEM_ID
{
    HC_SUBSYSTEM_ID_GAME_MIN = 0, // Start of the range of subsystem IDs available to titles.
    HC_SUBSYSTEM_ID_GAME = 0, // Primary subsystem ID used by the game
    HC_SUBSYSTEM_ID_GAME_MAX = 127, // End of the range of subsystem IDs available to titles.
    HC_SUBSYSTEM_ID_PLATFORM_RESERVED_MIN = 128, // Start of the range of subsystem IDs used by the system.
    HC_SUBSYSTEM_ID_XSAPI = 128, // Subsystem ID used by XSAPI internally
    HC_SUBSYSTEM_ID_XAL = 129, // Subsystem ID used by XAL internally
    HC_SUBSYSTEM_ID_XSAPI_COMPAT = 130, // Subsystem ID used by XSAPI compatibility layer internally
    HC_SUBSYSTEM_ID_PLATFORM_RESERVED_MAX = 191, // End of the range of subsystem IDs used by the system.
    HC_SUBSYSTEM_ID_MIDDLEWARE_RESERVED_MIN = 192, // Start of the range of subsystem IDs reserved for middleware.
    HC_SUBSYSTEM_ID_MIDDLEWARE_RESERVED_MAX = 255 // End of the range of subsystem IDs reserved for middleware.
} HC_SUBSYSTEM_ID;


#ifdef __cplusplus
#define HC_NOEXCEPT noexcept
#else
#define HC_NOEXCEPT
#endif


#if defined(__cplusplus)
} // end extern "C"
#endif // defined(__cplusplus)
