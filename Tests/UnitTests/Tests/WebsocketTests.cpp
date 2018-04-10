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

bool g_PerformMessageCallbackCalled = false;
void STDAPIVCALLTYPE PerformMessageCallback(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ PCSTR incomingBodyString
    )
{
    g_PerformMessageCallbackCalled = true;
}

bool g_PerformCloseCallbackCalled = false;
void STDAPIVCALLTYPE PerformCloseCallback(
    _In_ hc_websocket_handle_t websocket,
    _In_ HCWebSocketCloseStatus closeStatus
    )
{
    g_PerformCloseCallbackCalled = true;
}

_Ret_maybenull_ _Post_writable_byte_size_(size) void* STDAPIVCALLTYPE MemAlloc(
    _In_ size_t size,
    _In_ hc_memory_type memoryType
    );
void STDAPIVCALLTYPE MemFree(
    _In_ _Post_invalid_ void* pointer,
    _In_ hc_memory_type memoryType
    );
extern bool g_memAllocCalled;
extern bool g_memFreeCalled;




void Internal_HCWebSocketMessage(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ PCSTR incomingBodyString
    )
{
}

void Internal_HCWebSocketCloseEvent(
    _In_ hc_websocket_handle_t websocket,
    _In_ HCWebSocketCloseStatus closeStatus
)
{
}




bool g_HCWebSocketConnect_Called = false;
HRESULT Test_Internal_HCWebSocketConnect(
    _In_z_ PCSTR uri,
    _In_z_ PCSTR subProtocol,
    _In_ hc_websocket_handle_t websocket,
    _In_ AsyncBlock* asyncBlock
    )
{
    g_HCWebSocketConnect_Called = true;
    return S_OK;
}

bool g_HCWebSocketSendMessage_Called = false;
HRESULT Test_Internal_HCWebSocketSendMessage(
    _In_ hc_websocket_handle_t websocket,
    _In_z_ PCSTR message,
    _In_ AsyncBlock* asyncBlock
    )
{
    g_HCWebSocketSendMessage_Called = true;
    return S_OK;
}

bool g_HCWebSocketDisconnect_Called = false;
HRESULT Test_Internal_HCWebSocketDisconnect(
    _In_ hc_websocket_handle_t websocket,
    _In_ HCWebSocketCloseStatus closeStatus
    )
{
    g_HCWebSocketDisconnect_Called = true;
    return S_OK;
}

DEFINE_TEST_CLASS(WebsocketTests)
{
public:
    DEFINE_TEST_CLASS_PROPS(WebsocketTests);

    DEFINE_TEST_CASE(TestGlobalCallbacks)
    {
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        //g_PerformMessageCallbackCalled = false;
        //g_PerformCloseCallbackCalled = false;

        HCWebSocketMessageFunction messageFunc = nullptr;
        HCWebSocketCloseEventFunction closeFunc = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetFunctions(&messageFunc, &closeFunc));
        VERIFY_IS_NULL(messageFunc);
        VERIFY_IS_NULL(closeFunc);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetFunctions(Internal_HCWebSocketMessage, Internal_HCWebSocketCloseEvent));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetFunctions(&messageFunc, &closeFunc));
        VERIFY_IS_NOT_NULL(messageFunc);
        VERIFY_IS_NOT_NULL(closeFunc);
        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestCloseHandles)
    {
        g_memAllocCalled = false;
        g_memFreeCalled = false;
        VERIFY_ARE_EQUAL(S_OK, HCMemSetFunctions(&MemAlloc, &MemFree));
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());

        hc_websocket_handle_t websocket;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCreate(&websocket));

        hc_websocket_handle_t websocket2;
        websocket2 = HCWebSocketDuplicateHandle(websocket);
        VERIFY_IS_NOT_NULL(websocket2);
        g_memFreeCalled = false;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCloseHandle(websocket2));
        VERIFY_ARE_EQUAL(false, g_memFreeCalled);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCloseHandle(websocket));
        VERIFY_ARE_EQUAL(true, g_memFreeCalled);

        HCGlobalCleanup();
    }

    DEFINE_TEST_CASE(TestConnect)
    {
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());

        HCWebSocketConnectFunction websocketConnectFunc = nullptr;
        HCWebSocketSendMessageFunction websocketSendMessageFunc = nullptr;
        HCWebSocketDisconnectFunction websocketDisconnectFunc = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCGlobalGetWebSocketFunctions(&websocketConnectFunc, &websocketSendMessageFunc, &websocketDisconnectFunc));
        VERIFY_IS_NOT_NULL(websocketConnectFunc);
        VERIFY_IS_NOT_NULL(websocketSendMessageFunc);
        VERIFY_IS_NOT_NULL(websocketDisconnectFunc);

        VERIFY_ARE_EQUAL(S_OK, HCGlobalSetWebSocketFunctions(Test_Internal_HCWebSocketConnect, Test_Internal_HCWebSocketSendMessage, Test_Internal_HCWebSocketDisconnect));
        VERIFY_ARE_EQUAL(S_OK, HCGlobalGetWebSocketFunctions(&websocketConnectFunc, &websocketSendMessageFunc, &websocketDisconnectFunc));
        VERIFY_IS_NOT_NULL(websocketConnectFunc);
        VERIFY_IS_NOT_NULL(websocketSendMessageFunc);
        VERIFY_IS_NOT_NULL(websocketDisconnectFunc);

        hc_websocket_handle_t websocket;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCreate(&websocket));
        VERIFY_IS_NOT_NULL(websocket);

        const CHAR* proxy = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetProxyUri(websocket, &proxy));
        VERIFY_ARE_EQUAL_STR("", proxy);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetProxyUri(websocket, "1234"));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetProxyUri(websocket, &proxy));
        VERIFY_ARE_EQUAL_STR("1234", proxy);

        VERIFY_ARE_EQUAL(false, g_HCWebSocketConnect_Called);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketConnect("test", "subProtoTest", websocket, nullptr));
        VERIFY_ARE_EQUAL(true, g_HCWebSocketConnect_Called);

        VERIFY_ARE_EQUAL(false, g_HCWebSocketSendMessage_Called);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSendMessage(websocket, "test", nullptr));
        VERIFY_ARE_EQUAL(true, g_HCWebSocketSendMessage_Called);

        VERIFY_ARE_EQUAL(false, g_HCWebSocketDisconnect_Called);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketDisconnect(websocket));
        VERIFY_ARE_EQUAL(true, g_HCWebSocketDisconnect_Called);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCloseHandle(websocket));
        HCGlobalCleanup();
    }


    DEFINE_TEST_CASE(TestRequestHeaders)
    {
        DEFINE_TEST_CASE_PROPERTIES(TestRequestHeaders);
        VERIFY_ARE_EQUAL(S_OK, HCGlobalInitialize());
        hc_websocket_handle_t call = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCreate(&call));

        uint32_t numHeaders = 0;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(0, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetHeader(call, "testHeader", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        const CHAR* t1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetHeader(call, "testHeader", &t1));
        VERIFY_ARE_EQUAL_STR("testValue2", t1);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetHeader(call, "testHeader2", &t1));
        VERIFY_IS_NULL(t1);
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(1, numHeaders);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetHeader(call, "testHeader", "testValue"));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketSetHeader(call, "testHeader2", "testValue2"));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetNumHeaders(call, &numHeaders));
        VERIFY_ARE_EQUAL(2, numHeaders);

        const CHAR* hn0 = nullptr;
        const CHAR* hv0 = nullptr;
        const CHAR* hn1 = nullptr;
        const CHAR* hv1 = nullptr;
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetHeaderAtIndex(call, 0, &hn0, &hv0));
        VERIFY_ARE_EQUAL(S_OK, HCWebSocketGetHeaderAtIndex(call, 1, &hn1, &hv1));
        VERIFY_ARE_EQUAL_STR("testHeader", hn0);
        VERIFY_ARE_EQUAL_STR("testValue", hv0);
        VERIFY_ARE_EQUAL_STR("testHeader2", hn1);
        VERIFY_ARE_EQUAL_STR("testValue2", hv1);

        VERIFY_ARE_EQUAL(S_OK, HCWebSocketCloseHandle(call));
        HCGlobalCleanup();
    }

};

NAMESPACE_XBOX_HTTP_CLIENT_TEST_END
