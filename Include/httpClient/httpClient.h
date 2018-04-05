// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once
#include <httpClient/pal.h>
#include <httpClient/mock.h>
#include <httpClient/trace.h>
#include <httpClient/async.h>
#include <httpClient/asyncQueue.h>


/////////////////////////////////////////////////////////////////////////////////////////
// Memory APIs
//

/// <summary>
/// A callback invoked every time a new memory buffer must be dynamically allocated by the library.
/// This callback is optionally installed by calling HCMemSetFunctions()
/// 
/// The callback must allocate and return a pointer to a contiguous block of memory of the 
/// specified size that will remain valid until the app's corresponding HCMemFreeFunction 
/// callback is invoked to release it.
/// 
/// Every non-null pointer returned by this method will be subsequently passed to the corresponding
/// HCMemFreeFunction callback once the memory is no longer needed.
/// </summary>
/// <returns>A pointer to an allocated block of memory of the specified size, or a null 
/// pointer if allocation failed.</returns>
/// <param name="size">The size of the allocation to be made. This value will never be zero.</param>
/// <param name="memoryType">An opaque identifier representing the internal category of 
/// memory being allocated.</param>
typedef _Ret_maybenull_ _Post_writable_byte_size_(size) void*
(STDAPIVCALLTYPE* HCMemAllocFunction)(
    _In_ size_t size,
    _In_ hc_memory_type memoryType
    );

/// <summary>
/// A callback invoked every time a previously allocated memory buffer is no longer needed by 
/// the library and can be freed. This callback is optionally installed by calling HCMemSetFunctions()
///
/// The callback is invoked whenever the library has finished using a memory buffer previously 
/// returned by the app's corresponding HCMemAllocFunction such that the application can free the
/// memory buffer.
/// </summary>
/// <param name="pointer">The pointer to the memory buffer previously allocated. This value will
/// never be a null pointer.</param>
/// <param name="memoryType">An opaque identifier representing the internal category of 
/// memory being allocated.</param>
typedef void
(STDAPIVCALLTYPE* HCMemFreeFunction)(
    _In_ _Post_invalid_ void* pointer,
    _In_ hc_memory_type memoryType
    );

/// <summary>
/// Optionally sets the memory hook functions to allow callers to control route memory 
/// allocations to their own memory manager. This must be called before HCGlobalInitialize() 
/// and can not be called again until HCGlobalCleanup()
///
/// This method allows the application to install custom memory allocation routines in order 
/// to service all requests for new memory buffers instead of using default allocation routines.
///
/// The <paramref name="memAllocFunc" /> and <paramref name="memFreeFunc" /> parameters can be null
/// pointers to restore the default routines. Both callback pointers must be null or both must 
/// be non-null. Mixing custom and default routines is not permitted.
/// </summary>
/// <param name="memAllocFunc">A pointer to the custom allocation callback to use, or a null 
/// pointer to restore the default.</param>
/// <param name="memFreeFunc">A pointer to the custom freeing callback to use, or a null 
/// pointer to restore the default.</param>
STDAPI HCMemSetFunctions(
    _In_opt_ HCMemAllocFunction memAllocFunc,
    _In_opt_ HCMemFreeFunction memFreeFunc
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the memory hook functions to allow callers to control route memory allocations to their 
/// own memory manager.  This method allows the application get the default memory allocation routines.
/// This can be used along with HCMemSetFunctions() to monitor all memory allocations.
/// </summary>
/// <param name="memAllocFunc">Set to the current allocation callback.  Returns the default routine 
/// if not previously set</param>
/// <param name="memFreeFunc">Set to the to the current memory free callback.  Returns the default 
/// routine if not previously set</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCMemGetFunctions(
    _Out_ HCMemAllocFunction* memAllocFunc,
    _Out_ HCMemFreeFunction* memFreeFunc
    ) HC_NOEXCEPT;


/////////////////////////////////////////////////////////////////////////////////////////
// Global APIs
// 

/// <summary>
/// Initializes the library instance.
/// This must be called before any other method, except for HCMemSetFunctions() and HCMemGetFunctions()
/// Should have a corresponding call to HCGlobalCleanup().
/// </summary>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCGlobalInitialize() HC_NOEXCEPT;

/// <summary>
/// Immediately reclaims all resources associated with the library.
/// If you called HCMemSetFunctions(), call this before shutting down your app's memory manager.
/// </summary>
STDAPI_(void) HCGlobalCleanup() HC_NOEXCEPT;

/// <summary>
/// Returns the version of the library
/// </summary>
/// <param name="version">The version of the library in the format of release_year.release_month.date.rev.  
/// For example, 2017.07.20170710.01</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCGlobalGetLibVersion(_Outptr_ UTF8CSTR* version) HC_NOEXCEPT;


/////////////////////////////////////////////////////////////////////////////////////////
// Logging APIs
//

/// <summary>
/// Diagnostic level used by logging
/// </summary>
typedef enum HCLogLevel
{
    /// <summary>
    /// No logging
    /// </summary>
    HCLogLevel_Off,

    /// <summary>
    /// Log only errors
    /// </summary>
    HCLogLevel_Error,

    /// <summary>
    /// Log warnings and errors
    /// </summary>
    HCLogLevel_Warning,

    /// <summary>
    /// Log important, warnings and errors
    /// </summary>
    HCLogLevel_Important,

    /// <summary>
    /// Log info, important, warnings and errors
    /// </summary>
    HCLogLevel_Information,

    /// <summary>
    /// Log everything
    /// </summary>
    HCLogLevel_Verbose
} HCLogLevel;

/// <summary>
/// Sets the log level for the library.  Logs are sent the debug output
/// </summary>
/// <param name="logLevel">Log level</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCSettingsSetLogLevel(
    _In_ HCLogLevel logLevel
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the log level for the library
/// </summary>
/// <param name="logLevel">Log level</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCSettingsGetLogLevel(
    _Out_ HCLogLevel* logLevel
    ) HC_NOEXCEPT;


/////////////////////////////////////////////////////////////////////////////////////////
// Http APIs
//

/// <summary>
/// Creates an HTTP call handle
///
/// First create a HTTP handle using HCHttpCallCreate()
/// Then call HCHttpCallRequestSet*() to prepare the hc_call_handle_t
/// Then call HCHttpCallPerform() to perform HTTP call using the hc_call_handle_t.
/// This call is asynchronous, so the work will be done on a background thread and will return via the callback.
///
/// The perform call is asynchronous, so the work will be done on a background thread which calls 
/// DispatchAsyncQueue( ..., AsyncQueueCallbackType_Work ).  
///
/// The results will return to the callback on the thread that calls 
/// DispatchAsyncQueue( ..., AsyncQueueCallbackType_Completion ), then get the result of the HTTP call by calling 
/// HCHttpCallResponseGet*() to get the HTTP response of the hc_call_handle_t.
/// 
/// When the hc_call_handle_t is no longer needed, call HCHttpCallCloseHandle() to free the 
/// memory associated with the hc_call_handle_t
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallCreate(
    _Out_ hc_call_handle_t* call
    ) HC_NOEXCEPT;

/// <summary>
/// Perform HTTP call using the hc_call_handle_t
///
/// First create a HTTP handle using HCHttpCallCreate()
/// Then call HCHttpCallRequestSet*() to prepare the hc_call_handle_t
/// Then call HCHttpCallPerform() to perform HTTP call using the hc_call_handle_t.
/// This call is asynchronous, so the work will be done on a background thread and will return via the callback.
///
/// The perform call is asynchronous, so the work will be done on a background thread which calls 
/// DispatchAsyncQueue( ..., AsyncQueueCallbackType_Work ).  
///
/// The results will return to the callback on the thread that calls 
/// DispatchAsyncQueue( ..., AsyncQueueCallbackType_Completion ), then get the result of the HTTP call by calling 
/// HCHttpCallResponseGet*() to get the HTTP response of the hc_call_handle_t.
/// 
/// When the hc_call_handle_t is no longer needed, call HCHttpCallCloseHandle() to free the 
/// memory associated with the hc_call_handle_t
///
/// HCHttpCallPerform can only be called once.  Create new hc_call_handle_t to repeat the call.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="asyncBlock">The AsyncBlock that defines the async operation</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCHttpCallPerform(
    _In_ hc_call_handle_t call,
    _In_ AsyncBlock* asyncBlock
    ) HC_NOEXCEPT;

/// <summary>
/// Increments the reference count on the call object.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <returns>Returns the duplicated handle.</returns>
hc_call_handle_t HCHttpCallDuplicateHandle(
    _In_ hc_call_handle_t call
    ) HC_NOEXCEPT;

/// <summary>
/// Decrements the reference count on the call object. 
/// When the hc_call_handle_t ref count is 0, HCHttpCallCloseHandle() will 
/// free the memory associated with the hc_call_handle_t
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallCloseHandle(
    _In_ hc_call_handle_t call
    ) HC_NOEXCEPT;

/// <summary>
/// Returns a unique uint64_t which identifies this HTTP call object
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <returns>Returns a unique uint64_t which identifies this HTTP call object or 0 if invalid</returns>
STDAPI_(uint64_t)
HCHttpCallGetId(
    _In_ hc_call_handle_t call
    ) HC_NOEXCEPT;


/////////////////////////////////////////////////////////////////////////////////////////
// HttpCallRequest Set APIs
//

/// <summary>
/// Sets the url and method for the HTTP call
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="method">Method for the HTTP call</param>
/// <param name="url">URL for the HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetUrl(
    _In_ hc_call_handle_t call,
    _In_z_ UTF8CSTR method,
    _In_z_ UTF8CSTR url
    ) HC_NOEXCEPT;

/// <summary>
/// Set the request body bytes of the HTTP call
/// This must be called prior to calling HCHttpCallPerform.
/// </summary> 
/// <param name="call">The handle of the HTTP call</param>
/// <param name="requestBodyBytes">The request body bytes of the HTTP call.</param>
/// <param name="requestBodySize">The length in bytes of the body being set.</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetRequestBodyBytes(
    _In_ hc_call_handle_t call,
    _In_reads_bytes_(requestBodySize) const BYTE* requestBodyBytes,
    _In_ uint32_t requestBodySize
    ) HC_NOEXCEPT;

/// <summary>
/// Set the request body string of the HTTP call
/// This must be called prior to calling HCHttpCallPerform.
/// </summary> 
/// <param name="call">The handle of the HTTP call</param>
/// <param name="requestBodyString">The request body string of the HTTP call.</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetRequestBodyString(
    _In_ hc_call_handle_t call,
    _In_z_ UTF8CSTR requestBodyString
    ) HC_NOEXCEPT;

/// <summary>
/// Set a request header for the HTTP call
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="headerName">request header name for the HTTP call</param>
/// <param name="headerValue">request header value for the HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetHeader(
    _In_ hc_call_handle_t call,
    _In_z_ UTF8CSTR headerName,
    _In_z_ UTF8CSTR headerValue
    ) HC_NOEXCEPT;

/// <summary>
/// Sets if retry is allowed for this HTTP call
/// Defaults to true 
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call.  Pass nullptr to set the default for future calls</param>
/// <param name="retryAllowed">If retry is allowed for this HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetRetryAllowed(
    _In_opt_ hc_call_handle_t call,
    _In_ bool retryAllowed
    ) HC_NOEXCEPT;

/// <summary>
/// ID number of this REST endpoint used to cache the Retry-After header for fast fail.
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call.  Pass nullptr to set the default for future calls</param>
/// <param name="retryAfterCacheId">ID number of this REST endpoint used to cache the Retry-After header for fast fail.  1-1000 are reserved for XSAPI</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetRetryCacheId(
    _In_opt_ hc_call_handle_t call,
    _In_ uint32_t retryAfterCacheId
    ) HC_NOEXCEPT;

/// <summary>
/// Sets the timeout for this HTTP call.
/// Defaults to 30 seconds
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call.  Pass nullptr to set the default for future calls</param>
/// <param name="timeoutInSeconds">The timeout for this HTTP call.</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetTimeout(
    _In_opt_ hc_call_handle_t call,
    _In_ uint32_t timeoutInSeconds
    ) HC_NOEXCEPT;

/// <summary>
/// Sets the HTTP retry delay in seconds. The default and minimum delay is 2 seconds.
/// 
/// Retries are delayed using a exponential back off.  By default, it will delay 2 seconds then the 
/// next retry will delay 4 seconds, then 8 seconds, and so on up to a max of 1 min until either
/// the call succeeds or the HTTP timeout window is reached, at which point the call will fail.
/// The delay is also jittered between the current and next delay to spread out service load.
/// The default for the HTTP timeout window is 20 seconds and can be changed using HCSettingsSetTimeoutWindow()
/// 
/// If the service returns an an HTTP error with a "Retry-After" header, then all future calls to that API 
/// will immediately fail with the original error without contacting the service until the "Retry-After" 
/// time has been reached.
///
/// Idempotent service calls are retried when a network error occurs or the server responds with 
/// one of these HTTP status codes:
/// 408 (Request Timeout)
/// 429 (Too Many Requests)
/// 500 (Internal Server Error)
/// 502 (Bad Gateway)
/// 503 (Service Unavailable)
/// 504 (Gateway Timeout)
///
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call.  Pass nullptr to set the default for future calls</param>
/// <param name="retryDelayInSeconds">The retry delay in seconds</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetRetryDelay(
    _In_opt_ hc_call_handle_t call,
    _In_ uint32_t retryDelayInSeconds
    ) HC_NOEXCEPT;

/// <summary>
/// Sets the HTTP timeout window in seconds.
///
/// This controls how long to spend attempting to retry idempotent service calls before failing.
/// The default is 20 seconds
///
/// Idempotent service calls are retried when a network error occurs or the server responds 
/// with one of these HTTP status codes:
/// 408 (Request Timeout)
/// 429 (Too Many Requests)
/// 500 (Internal Server Error)
/// 502 (Bad Gateway)
/// 503 (Service Unavailable)
/// 504 (Gateway Timeout)
///
/// This must be called prior to calling HCHttpCallPerform.
/// </summary>
/// <param name="call">The handle of the HTTP call.  Pass nullptr to set the default for future calls</param>
/// <param name="timeoutWindowInSeconds">The timeout window in seconds</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, or E_FAIL.</returns>
STDAPI HCHttpCallRequestSetTimeoutWindow(
    _In_opt_ hc_call_handle_t call,
    _In_ uint32_t timeoutWindowInSeconds
    ) HC_NOEXCEPT;


/////////////////////////////////////////////////////////////////////////////////////////
// HttpCallResponse Get APIs
// 

/// <summary>
/// Get the response body string of the HTTP call
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="responseString">
/// The response body string of the HTTP call
/// The memory for the returned string pointer remains valid for the life of the hc_call_handle_t object until HCHttpCallCloseHandle() is called on it.
/// </param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallResponseGetResponseString(
    _In_ hc_call_handle_t call,
    _Out_ UTF8CSTR* responseString
    ) HC_NOEXCEPT;

/// <summary>
/// Get the HTTP status code of the HTTP call response
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="statusCode">the HTTP status code of the HTTP call response</param>
STDAPI HCHttpCallResponseGetStatusCode(
    _In_ hc_call_handle_t call,
    _Out_ uint32_t* statusCode
    );

/// <summary>
/// Get the network error code of the HTTP call
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="networkErrorCode">The network error code of the HTTP call. Possible values are S_OK, or E_FAIL.</param>
/// <param name="platformNetworkErrorCode">The platform specific network error code of the HTTP call to be used for logging / debugging</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallResponseGetNetworkErrorCode(
    _In_ hc_call_handle_t call,
    _Out_ HRESULT* networkErrorCode,
    _Out_ uint32_t* platformNetworkErrorCode
    ) HC_NOEXCEPT;

/// <summary>
/// Get a response header for the HTTP call for a given header name
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="headerName">Response header name for the HTTP call
/// The memory for the returned string pointer remains valid for the life of the hc_call_handle_t object until HCHttpCallCloseHandle() is called on it.
/// </param>
/// <param name="headerValue">Response header value for the HTTP call.
/// Returns nullptr if the header doesn't exist.
/// The memory for the returned string pointer remains valid for the life of the hc_call_handle_t object until HCHttpCallCloseHandle() is called on it.
/// </param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallResponseGetHeader(
    _In_ hc_call_handle_t call,
    _In_z_ UTF8CSTR headerName,
    _Out_ UTF8CSTR* headerValue
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the number of response headers in the HTTP call
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="numHeaders">The number of response headers in the HTTP call</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallResponseGetNumHeaders(
    _In_ hc_call_handle_t call,
    _Out_ uint32_t* numHeaders
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the response headers at specific zero based index in the HTTP call.
/// Use HCHttpCallResponseGetNumHeaders() to know how many response headers there are in the HTTP call.
/// This can only be called after calling HCHttpCallPerform when the HTTP task is completed.
/// </summary>
/// <param name="call">The handle of the HTTP call</param>
/// <param name="headerIndex">Specific zero based index of the response header</param>
/// <param name="headerName">Response header name for the HTTP call.
/// The memory for the returned string pointer remains valid for the life of the hc_call_handle_t object until HCHttpCallCloseHandle() is called on it.
/// </param>
/// <param name="headerValue">Response header value for the HTTP call.
/// The memory for the returned string pointer remains valid for the life of the hc_call_handle_t object until HCHttpCallCloseHandle() is called on it.
/// </param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCHttpCallResponseGetHeaderAtIndex(
    _In_ hc_call_handle_t call,
    _In_ uint32_t headerIndex,
    _Out_ UTF8CSTR* headerName,
    _Out_ UTF8CSTR* headerValue
    ) HC_NOEXCEPT;

/////////////////////////////////////////////////////////////////////////////////////////
// WebSocket APIs
// 

/// <summary>
/// Creates an WebSocket handle
///
/// WebSocket usage:
/// Setup the handler functions with HCWebSocketSetFunctions()
/// Create a WebSocket handle using HCWebSocketCreate()
/// Call HCWebSocketSetProxyUri() and HCWebSocketSetHeader() to prepare the hc_websocket_handle_t
/// Call HCWebSocketConnect() to connect the WebSocket using the hc_websocket_handle_t.
/// Call HCWebSocketSendMessage() to send a message to the WebSocket using the hc_websocket_handle_t.
/// Call HCWebSocketDisconnect() to disconnect the WebSocket using the hc_websocket_handle_t.
/// Call HCWebSocketCloseHandle() when done with the hc_websocket_handle_t to free the associated memory
/// </summary>
/// <param name="websocket">The handle of the websocket</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCWebSocketCreate(
    _Out_ hc_websocket_handle_t* websocket
    ) HC_NOEXCEPT;

/// <summary>
/// Set the proxy URI for the WebSocket
/// This must be called prior to calling HCWebSocketConnect.
/// </summary>
/// <param name="websocket">The handle of the WebSocket</param>
/// <param name="proxyUri">The proxy URI for the WebSocket</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCWebSocketSetProxyUri(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ UTF8CSTR proxyUri
    ) HC_NOEXCEPT;

/// <summary>
/// Set a header for the WebSocket
/// This must be called prior to calling HCWebSocketConnect.
/// </summary>
/// <param name="websocket">The handle of the WebSocket</param>
/// <param name="headerName">Header name for the WebSocket</param>
/// <param name="headerValue">Header value for the WebSocket</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCWebSocketSetHeader(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ UTF8CSTR headerName,
    _In_z_ UTF8CSTR headerValue
    ) HC_NOEXCEPT;


/// <summary>
/// A callback invoked every time a WebSocket receives an incoming message
/// </summary>
/// <param name="websocket">Handle to the WebSocket that this message was sent to</param>
/// <param name="incomingBodyString">Body of the incoming message as a string value, only if the message type is UTF-8.</param>
typedef void
(STDAPIVCALLTYPE* HCWebSocketMessageFunction)(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ UTF8CSTR incomingBodyString
    );

/// <summary>
/// A callback invoked when a WebSocket is closed
/// </summary>
/// <param name="websocket">Handle to the WebSocket</param>
/// <param name="closeStatus">The status of why the WebSocket was closed</param>
typedef void
(STDAPIVCALLTYPE* HCWebSocketCloseEventFunction)(
    _In_ hc_websocket_handle_t websocket,
    _In_ HCWebSocketCloseStatus closeStatus
    );

/// <summary>
/// Sets the WebSocket functions to allow callers to respond to incoming messages and WebSocket close events.
/// </summary>
/// <param name="messageFunc">A pointer to the message handling callback to use, or a null pointer to remove.</param>
/// <param name="closeFunc">A pointer to the close callback to use, or a null pointer to remove.</param>
STDAPI HCWebSocketSetFunctions(
    _In_opt_ HCWebSocketMessageFunction messageFunc,
    _In_opt_ HCWebSocketCloseEventFunction closeFunc
    ) HC_NOEXCEPT;


/// <summary>
/// Used by HCWebSocketConnect() and HCWebSocketSendMessage()
/// </summary>
typedef struct WebSocketCompletionResult
{
    /// <param name="websocket">The handle of the HTTP call</param>
    hc_websocket_handle_t websocket;

    /// <param name="errorCode">The error code of the call. Possible values are S_OK, or E_FAIL.</param>
    HRESULT errorCode;

    /// <param name="platformErrorCode">The platform specific network error code of the call to be used for logging / debugging</param>
    uint32_t platformErrorCode;
} WebSocketCompletionResult;

/// <summary>
/// Connects to the WebSocket.
/// On UWP and XDK, the connection thread is owned and controlled by Windows::Networking::Sockets::MessageWebSocket
/// </summary>
/// <param name="uri">The URI to connect to</param>
/// <param name="subProtocol">The subProtocol to connect to</param>
/// <param name="websocket">The handle of the WebSocket</param>
/// <param name="asyncBlock">The AsyncBlock that defines the async operation</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCWebSocketConnect(
    _In_z_ UTF8CSTR uri,
    _In_z_ UTF8CSTR subProtocol,
    _In_ hc_websocket_handle_t websocket,
    _In_ AsyncBlock* asyncBlock
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the result for HCGetWebSocketConnectResult.
/// </summary>
/// <param name="asyncBlock">The AsyncBlock that defines the async operation</param>
/// <param name="result">Pointer to the result payload</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, E_OUTOFMEMORY, or E_FAIL.</returns>
STDAPI HCGetWebSocketConnectResult(
    _In_ AsyncBlock* asyncBlock,
    _In_ WebSocketCompletionResult* result
    ) HC_NOEXCEPT;

/// <summary>
/// Send message the WebSocket
/// </summary>
/// <param name="websocket">Handle to the WebSocket</param>
/// <param name="message">The message to send</param>
/// <param name="asyncBlock">The AsyncBlock that defines the async operation</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCWebSocketSendMessage(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ UTF8CSTR message,
    _In_ AsyncBlock* asyncBlock
    ) HC_NOEXCEPT;

/// <summary>
/// Gets the result from HCWebSocketSendMessage 
/// </summary>
/// <param name="asyncBlock">The AsyncBlock that defines the async operation</param>
/// <param name="result">Pointer to the result payload</param>
/// <returns>Returns the duplicated handle.</returns>
STDAPI HCGetWebSocketSendMessageResult(
    _In_ AsyncBlock* asyncBlock,
    _In_ WebSocketCompletionResult* result
    ) HC_NOEXCEPT;

/// <summary>
/// Disconnects / closes the WebSocket
/// </summary>
/// <param name="websocket">Handle to the WebSocket</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCWebSocketDisconnect(
    _In_ hc_websocket_handle_t websocket
    ) HC_NOEXCEPT;

/// <summary>
/// Increments the reference count on the call object.
/// </summary>
/// <param name="websocket">Handle to the WebSocket</param>
/// <returns>Returns the duplicated handle.</returns>
hc_websocket_handle_t HCWebSocketDuplicateHandle(
    _In_ hc_websocket_handle_t websocket
    ) HC_NOEXCEPT;

/// <summary>
/// Decrements the reference count on the WebSocket object. 
/// When the ref count is 0, HCWebSocketCloseHandle() will 
/// free the memory associated with the hc_websocket_handle_t
/// </summary>
/// <param name="websocket">Handle to the WebSocket</param>
/// <returns>Result code for this API operation.  Possible values are S_OK, E_INVALIDARG, or E_FAIL.</returns>
STDAPI HCWebSocketCloseHandle(
    _In_ hc_websocket_handle_t websocket
    ) HC_NOEXCEPT;



