﻿// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "UnitTestIncludes.h"
#define TEST_CLASS_OWNER L"jasonsa"
#include "DefineTestMacros.h"
#include "Utils.h"
#include "../global/global.h"

using namespace xbox::httpclient;

NAMESPACE_XBOX_HTTP_CLIENT_TEST_BEGIN

#define VERIFY_EXCEPTION_TO_HR(x,hrVerify) \
        try \
        { \
            throw x; \
        } \
        catch (...) \
        { \
            HRESULT hr = utils::convert_exception_to_hresult(); \
            VERIFY_ARE_EQUAL(hr, hrVerify); \
        }

bool g_memAllocCalled = false;
bool g_memFreeCalled = false;

_Ret_maybenull_ _Post_writable_byte_size_(size) void* STDAPIVCALLTYPE MemAlloc(
    _In_ size_t size,
    _In_ hc_memory_type memoryType
    )   
{
    g_memAllocCalled = true;
    return new (std::nothrow) int8_t[size];
}

void STDAPIVCALLTYPE MemFree(
    _In_ _Post_invalid_ void* pointer,
    _In_ hc_memory_type memoryType
    )
{
    g_memFreeCalled = true;
    delete[] pointer;
}

static bool g_PerformCallbackCalled = false;
static void STDAPIVCALLTYPE PerformCallback(
    _In_ hc_call_handle call,
    _In_ AsyncBlock* asyncBlock
    )
{
    g_PerformCallbackCalled = true;
    CompleteAsync(asyncBlock, S_OK, 0);
}


DEFINE_TEST_CLASS(HttpTests)
{
public:
    DEFINE_TEST_CLASS_PROPS(HttpTests);

    DEFINE_TEST_CASE(TestMem)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestMem);
        g_memAllocCalled = false;
        g_memFreeCalled = false;

        VERIFY_ARE_EQUAL(S_OK, HCMemSetFunctions(&MemAlloc, &MemFree));

        {
            http_internal_vector<int> v;
            v.reserve(10000);

            VERIFY_ARE_EQUAL(true, g_memAllocCalled);
            VERIFY_ARE_EQUAL(false, g_memFreeCalled);
            g_memAllocCalled = false;
            g_memFreeCalled = false;
        }
        VERIFY_ARE_EQUAL(false, g_memAllocCalled);
        VERIFY_ARE_EQUAL(true, g_memFreeCalled);

        HCMemAllocFunction memAllocFunc = nullptr;
        HCMemFreeFunction memFreeFunc = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCMemGetFunctions(&memAllocFunc, &memFreeFunc));
        VERIFY_IS_NOT_NULL(memAllocFunc);
        VERIFY_IS_NOT_NULL(memFreeFunc);

        VERIFY_ARE_EQUAL(S_OK, HCMemSetFunctions(nullptr, nullptr));

        g_memAllocCalled = false;
        g_memFreeCalled = false;
        {
            http_internal_vector<int> v;
            v.reserve(10000);

            VERIFY_ARE_EQUAL(false, g_memAllocCalled);
            VERIFY_ARE_EQUAL(false, g_memFreeCalled);
        }
        VERIFY_ARE_EQUAL(false, g_memAllocCalled);
        VERIFY_ARE_EQUAL(false, g_memFreeCalled);
    }

    DEFINE_TEST_CASE(TestGlobalInit)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestGlobalInit);

        VERIFY_IS_NULL(get_http_singleton(false));
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        VERIFY_IS_NOT_NULL(get_http_singleton(false));
        HCGlobalCleanup();
        VERIFY_IS_NULL(get_http_singleton(false));
    }

    DEFINE_TEST_CASE(TestGlobalPerformCallback)
    {
        DEFINE_TEST_CASE_PROPERTIES_FOCUS(TestGlobalPerformCallback);

        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        g_PerformCallbackCalled = false;
        HCCallPerformFunction func = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCGlobalGetHttpCallPerformFunction(&func));
        VERIFY_IS_NOT_NULL(func);

        HCGlobalSetHttpCallPerformFunction(&PerformCallback);
        hc_call_handle call;
        HCHttpCallCreate(&call);
        VERIFY_ARE_EQUAL(false, g_PerformCallbackCalled);

        async_queue_t queue;
        uint32_t sharedAsyncQueueId = 0;
        CreateSharedAsyncQueue(
            sharedAsyncQueueId,
            AsyncQueueDispatchMode::AsyncQueueDispatchMode_Manual,
            AsyncQueueDispatchMode::AsyncQueueDispatchMode_Manual,
            &queue);

        AsyncBlock* asyncBlock = new AsyncBlock;
        ZeroMemory(asyncBlock, sizeof(AsyncBlock));
        asyncBlock->context = call;
        asyncBlock->queue = queue;
        asyncBlock->callback = [](AsyncBlock* asyncBlock)
        {
            HRESULT errCode = S_OK;
            uint32_t platErrCode = 0;
            hc_call_handle call = static_cast<hc_call_handle>(asyncBlock->context);
            VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNetworkErrorCode(call, &errCode, &platErrCode));
            uint32_t statusCode = 0;
            VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetStatusCode(call, &statusCode));
            delete asyncBlock;
        };
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallPerform(call, asyncBlock));

        while (true)
        {
            if (!DispatchAsyncQueue(queue, AsyncQueueCallbackType::AsyncQueueCallbackType_Work, 0)) break;
        }
        VERIFY_ARE_EQUAL(true, DispatchAsyncQueue(queue, AsyncQueueCallbackType::AsyncQueueCallbackType_Completion, 0));
        VERIFY_ARE_EQUAL(true, g_PerformCallbackCalled);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        CloseAsyncQueue(queue);
        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestSettings)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestSettings);
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());

        HCLogLevel level;

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Off));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Off, level);

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Error));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Error, level);

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Warning));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Warning, level);

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Important));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Important, level);

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Information));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Information, level);

        VERIFY_ARE_EQUAL(S_OK, HCSettingsSetLogLevel(HCLogLevel_Verbose));
        VERIFY_ARE_EQUAL(S_OK, HCSettingsGetLogLevel(&level));
        VERIFY_ARE_EQUAL(HCLogLevel_Verbose, level);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetTimeoutWindow(nullptr, 1000));
        uint32_t timeout = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetTimeoutWindow(nullptr, &timeout));
        VERIFY_ARE_EQUAL(1000, timeout);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetRetryDelay(nullptr, 500));
        uint32_t retryDelayInSeconds = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetRetryDelay(nullptr, &retryDelayInSeconds));

        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestCall)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestCall);

        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_call_handle call = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCreate(&call));
        VERIFY_IS_NOT_NULL(call);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestRequest)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestRequest);
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_call_handle call = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCreate(&call));

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetUrl(call, "1", "2"));
        const CHAR* t1 = nullptr;
        const CHAR* t2 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetUrl(call, &t1, &t2));
        VERIFY_ARE_EQUAL_STR("1", t1);
        VERIFY_ARE_EQUAL_STR("2", t2);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetRequestBodyString(call, "4"));
        const BYTE* s1 = 0;
        uint32_t bodySize = 0;
        const CHAR* t3 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetRequestBodyString(call, &t3));
        VERIFY_ARE_EQUAL_STR("4", t3);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetRequestBodyBytes(call, &s1, &bodySize));
        VERIFY_ARE_EQUAL(bodySize, 1);
        VERIFY_ARE_EQUAL(s1[0], '4');
        std::string s2( reinterpret_cast<char const*>(s1), bodySize);
        VERIFY_ARE_EQUAL_STR("4", s2.c_str());

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetRetryAllowed(call, true));
        bool retry = false;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetRetryAllowed(call, &retry));
        VERIFY_ARE_EQUAL(true, retry);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetTimeout(call, 2000));
        uint32_t timeout = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetTimeout(call, &timeout));
        VERIFY_ARE_EQUAL(2000, timeout);
                
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetTimeoutWindow(call, 1000));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetTimeoutWindow(call, &timeout));
        VERIFY_ARE_EQUAL(1000, timeout);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetRetryDelay(call, 500));
        uint32_t retryDelayInSeconds = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetRetryDelay(call, &retryDelayInSeconds));

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        HCGlobalCleanup();
    }


    DEFINE_TEST_CASE(TestRequestHeaders)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestRequestHeaders);
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_call_handle call = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCreate(&call));

        uint32_t numHeaders = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(0, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetHeader(call, "testHeader", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        const CHAR* t1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetHeader(call, "testHeader", &t1));
        VERIFY_ARE_EQUAL_STR("testValue2", t1);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetHeader(call, "testHeader2", &t1));
        VERIFY_IS_NULL(t1);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestSetHeader(call, "testHeader2", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(2, numHeaders);

        const CHAR* hn0 = nullptr;
        const CHAR* hv0 = nullptr;
        const CHAR* hn1 = nullptr;
        const CHAR* hv1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetHeaderAtIndex(call, 0, &hn0, &hv0));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallRequestGetHeaderAtIndex(call, 1, &hn1, &hv1));
        VERIFY_ARE_EQUAL_STR("testHeader", hn0);
        VERIFY_ARE_EQUAL_STR("testValue", hv0);
        VERIFY_ARE_EQUAL_STR("testHeader2", hn1);
        VERIFY_ARE_EQUAL_STR("testValue2", hv1);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestResponse)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestResponse);

        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_call_handle call = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCreate(&call));

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetResponseString(call, "test1"));
        const CHAR* t1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetResponseString(call, &t1));
        VERIFY_ARE_EQUAL_STR("test1", t1);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetStatusCode(call, 200));
        uint32_t statusCode = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetStatusCode(call, &statusCode));
        VERIFY_ARE_EQUAL(200, statusCode);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetNetworkErrorCode(call, E_OUTOFMEMORY, 101));
        HRESULT errCode = S_OK;
        uint32_t errorCode = 0;
        uint32_t platErrorCode = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNetworkErrorCode(call, &errCode, &platErrorCode));
        VERIFY_ARE_EQUAL(101, platErrorCode);
        VERIFY_ARE_EQUAL(E_OUTOFMEMORY, errCode);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestResponseHeaders)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestResponseHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_call_handle call = nullptr;
        HCHttpCallCreate(&call);

        uint32_t numHeaders = 0;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(0, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetHeader(call, "testHeader", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        const CHAR* t1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetHeader(call, "testHeader", &t1));
        VERIFY_ARE_EQUAL_STR("testValue2", t1);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetHeader(call, "testHeader2", &t1));
        VERIFY_IS_NULL(t1);
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseSetHeader(call, "testHeader2", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(2, numHeaders);

        const CHAR* hn0 = nullptr;
        const CHAR* hv0 = nullptr;
        const CHAR* hn1 = nullptr;
        const CHAR* hv1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetHeaderAtIndex(call, 0, &hn0, &hv0));
        VERIFY_ARE_EQUAL(S_OK, HCHttpCallResponseGetHeaderAtIndex(call, 1, &hn1, &hv1));
        VERIFY_ARE_EQUAL_STR("testHeader", hn0);
        VERIFY_ARE_EQUAL_STR("testValue", hv0);
        VERIFY_ARE_EQUAL_STR("testHeader2", hn1);
        VERIFY_ARE_EQUAL_STR("testValue2", hv1);

        VERIFY_ARE_EQUAL(S_OK, HCHttpCallCloseHandle(call));
        HCGlobalCleanup();
    }
};

NAMESPACE_XBOX_HTTP_CLIENT_TEST_END
