// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include "../hcwebsocket.h"

using namespace xbox::httpclient;
using namespace ::Windows::Foundation;
using namespace ::Windows::Storage;
using namespace ::Windows::Storage::Streams;
using namespace ::Windows::Networking;
using namespace ::Windows::Networking::Sockets;


class websocket_outgoing_message
{
public: 
    http_internal_string m_message;
    HC_TASK_HANDLE m_taskHandle;
    AsyncBlock* m_asyncBlock;
    DataWriterStoreOperation^ m_storeAsyncOp;
    AsyncStatus m_storeAsyncOpStatus;
    HRESULT m_storeAsyncResult;
    uint64_t m_id;
};

// This class is required by the implementation in order to function:
// The TypedEventHandler requires the message received and close handler to be a member of WinRT class.
ref class ReceiveContext sealed
{
public:
    ReceiveContext() : m_websocket(nullptr)
    {
    }

    friend HRESULT WebsocketConnectDoWork(
        _In_opt_ void* executionRoutineContext,
        _In_ AsyncBlock* asyncBlock);

    void OnReceive(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args);
    void OnClosed(IWebSocket^ sender, WebSocketClosedEventArgs^ args);

private:
    HC_WEBSOCKET_HANDLE m_websocket;
};

class winrt_websocket_task : public xbox::httpclient::hc_task
{
public:
    winrt_websocket_task() : m_connectAsyncOpResult(S_OK)
    {
    }

    Windows::Networking::Sockets::MessageWebSocket^ m_messageWebSocket;
    Windows::Storage::Streams::DataWriter^ m_messageDataWriter;
    HRESULT m_connectAsyncOpResult;
    ReceiveContext^ m_context;

    IAsyncAction^ m_connectAsyncOp;
    AsyncStatus m_connectAsyncOpStatus;

    std::mutex m_outgoingMessageQueueLock;
    std::queue<std::shared_ptr<websocket_outgoing_message>> m_outgoingMessageQueue;
    HC_WEBSOCKET_HANDLE m_websocketHandle;
};

void MessageWebSocketSendMessage(
    _In_ std::shared_ptr<winrt_websocket_task> websocketTask
    );

void ReceiveContext::OnReceive(MessageWebSocket^ sender, MessageWebSocketMessageReceivedEventArgs^ args)
{
    try
    {
        DataReader^ reader = args->GetDataReader();
        const auto len = reader->UnconsumedBufferLength;
        if (len > 0)
        {
            std::string payload;
            payload.resize(len);
            reader->ReadBytes(Platform::ArrayReference<uint8_t>(reinterpret_cast<uint8 *>(&payload[0]), len));
            HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: receieved msg [%s]", m_websocket->id, payload.c_str());

            HC_WEBSOCKET_MESSAGE_FUNC messageFunc = nullptr;
            HCWebSocketGetFunctions(&messageFunc, nullptr);
            if (messageFunc != nullptr)
            {
                messageFunc(m_websocket, payload.c_str());
            }
        }
    }
    catch (Platform::Exception ^e)
    {
    }
    catch (...)
    {
    }
}

void ReceiveContext::OnClosed(IWebSocket^ sender, WebSocketClosedEventArgs^ args)
{
    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: on closed event triggered", m_websocket->id);

    HC_WEBSOCKET_CLOSE_EVENT_FUNC closeFunc = nullptr;
    HCWebSocketGetFunctions(nullptr, &closeFunc);
    if (closeFunc != nullptr)
    {
        closeFunc(m_websocket, static_cast<HC_WEBSOCKET_CLOSE_STATUS>(args->Code));
    }
}

inline bool str_icmp(const char* left, const char* right)
{
#ifdef _WIN32
    return _stricmp(left, right) == 0;
#else
    return boost::iequals(left, right);
#endif
}

http_internal_vector<http_internal_wstring> parse_subprotocols(const http_internal_string& subProtocol)
{
    http_internal_vector<http_internal_wstring> values;
    http_internal_wstring token;
    std::wstringstream header(utf16_from_utf8(subProtocol).c_str());

    while (std::getline(header, token, L','))
    {
        trim_whitespace(token);
        if (!token.empty())
        {
            values.push_back(token);
        }
    }

    return values;
}

HRESULT WebsocketConnectDoWork(
    _In_opt_ void* executionRoutineContext,
    _In_ AsyncBlock* asyncBlock
    )
try
{
    HC_WEBSOCKET_HANDLE websocket = static_cast<HC_WEBSOCKET_HANDLE>(executionRoutineContext);
    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Connect executing", websocket->id);

    std::shared_ptr<winrt_websocket_task> websocketTask = std::dynamic_pointer_cast<winrt_websocket_task>(websocket->task);
    websocketTask->m_messageWebSocket = ref new MessageWebSocket();

    uint32_t numHeaders = 0;
    HCWebSocketGetNumHeaders(websocket, &numHeaders);

    http_internal_string protocolHeader("Sec-WebSocket-Protocol");
    for (uint32_t i = 0; i < numHeaders; i++)
    {
        const char* headerName;
        const char* headerValue;
        HCWebSocketGetHeaderAtIndex(websocket, i, &headerName, &headerValue);

        // The MessageWebSocket API throws a COMException if you try to set the
        // 'Sec-WebSocket-Protocol' header here. It requires you to go through their API instead.
        if (headerName != nullptr && headerValue != nullptr && !str_icmp(headerName, protocolHeader.c_str()))
        {
            http_internal_wstring wHeaderName = utf16_from_utf8(headerName);
            http_internal_wstring wHeaderValue = utf16_from_utf8(headerValue);
            websocketTask->m_messageWebSocket->SetRequestHeader(
                Platform::StringReference(wHeaderName.c_str()),
                Platform::StringReference(wHeaderValue.c_str()));
            HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Header %d [%s: %s]", websocket->id, i, headerName, headerValue);
        }
    }

    auto protocols = parse_subprotocols(websocket->subProtocol);
    for (const auto& value : protocols)
    {
        websocketTask->m_messageWebSocket->Control->SupportedProtocols->Append(Platform::StringReference(value.c_str()));
        HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Protocol [%S]", websocket->id, value.c_str());
    }

    websocketTask->m_context = ref new ReceiveContext();
    websocketTask->m_context->m_websocket = websocket;

    http_internal_wstring aUrl = utf16_from_utf8(websocket->uri);
    const auto cxUri = ref new Windows::Foundation::Uri(Platform::StringReference(aUrl.c_str()));

    websocketTask->m_messageWebSocket->MessageReceived += ref new TypedEventHandler<MessageWebSocket^, MessageWebSocketMessageReceivedEventArgs^>(websocketTask->m_context, &ReceiveContext::OnReceive);
    websocketTask->m_messageWebSocket->Closed += ref new TypedEventHandler<IWebSocket^, WebSocketClosedEventArgs^>(websocketTask->m_context, &ReceiveContext::OnClosed);

    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: connecting to %s", websocket->id, websocket->uri.c_str());
    websocketTask->m_connectAsyncOp = websocketTask->m_messageWebSocket->ConnectAsync(cxUri);

    websocketTask->m_connectAsyncOp->Completed = ref new AsyncActionCompletedHandler(
        [websocket, websocketTask, asyncBlock](
            Windows::Foundation::IAsyncAction^ asyncOp, 
            Windows::Foundation::AsyncStatus status)
    {
        UNREFERENCED_PARAMETER(status);
        try
        {
            websocketTask->m_messageDataWriter = ref new DataWriter(websocketTask->m_messageWebSocket->OutputStream);
            if (status == Windows::Foundation::AsyncStatus::Error)
            {
                websocketTask->m_connectAsyncOpResult = E_FAIL;
            }
            else
            {
                websocketTask->m_connectAsyncOpResult = S_OK;
            }
        }
        catch (Platform::Exception^ e)
        {
            websocketTask->m_connectAsyncOpResult = e->HResult;
        }
        catch (...)
        {
            websocketTask->m_connectAsyncOpResult = E_FAIL;
        }
        if (FAILED(websocketTask->m_connectAsyncOpResult))
        {
            HC_TRACE_ERROR(WEBSOCKET, "Websocket [ID %llu]: connect failed 0x%0.8x", websocket->id, websocketTask->m_connectAsyncOpResult);
        }
        else
        {
            HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu] connect complete", websocket->id);
        }

        CompleteAsync(asyncBlock, S_OK, sizeof(WebSocketCompletionResult));
    });

    return E_PENDING;
}
CATCH_RETURN()

HRESULT WebsocketConnectGetResult(_Inout_ AsyncProviderData* data)
{
    HC_WEBSOCKET_HANDLE websocket = static_cast<HC_WEBSOCKET_HANDLE>(data->context);
    std::shared_ptr<winrt_websocket_task> websocketTask = std::dynamic_pointer_cast<winrt_websocket_task>(websocket->task);

    WebSocketCompletionResult result = {};
    result.websocket = websocket;
    result.errorCode = (FAILED(websocketTask->m_connectAsyncOpResult)) ? HC_E_FAIL : HC_OK;
    result.platformErrorCode = websocketTask->m_connectAsyncOpResult;
    CopyMemory(data->buffer, &result, sizeof(WebSocketCompletionResult));

    return S_OK;
}

HC_RESULT Internal_HCWebSocketConnect(
    _In_z_ PCSTR uri,
    _In_z_ PCSTR subProtocol,
    _In_ HC_WEBSOCKET_HANDLE websocket,
    _In_ AsyncBlock* asyncBlock
    )
{
    std::shared_ptr<winrt_websocket_task> websocketTask = std::make_shared<winrt_websocket_task>();
    websocketTask->m_websocketHandle = websocket;
    websocket->uri = uri;
    websocket->subProtocol = subProtocol;
    websocket->task = std::dynamic_pointer_cast<xbox::httpclient::hc_task>(websocketTask);

    HC_RESULT hr = HRESULTtoHC(BeginAsync(asyncBlock, websocket, HCWebSocketConnect, __FUNCTION__,
        [](_In_ AsyncOp op, _Inout_ AsyncProviderData* data)
    {
        switch (op)
        {
            case AsyncOp_DoWork: return WebsocketConnectDoWork(data->context, data->async);
            case AsyncOp_GetResult: return WebsocketConnectGetResult(data);
        }

        return S_OK;
    }));

    if (hr == HC_OK)
    {
        hr = HRESULTtoHC(ScheduleAsync(asyncBlock, 0));
    }

    return hr;
}

HC_RESULT Internal_HCWebSocketSendMessage(
    _In_ HC_WEBSOCKET_HANDLE websocket,
    _In_z_ PCSTR message,
    _In_ AsyncBlock* asyncBlock
    )
{
    if (message == nullptr)
    {
        return HC_E_INVALIDARG;
    }

    auto httpSingleton = get_http_singleton(false);
    if (nullptr == httpSingleton)
        return HC_E_NOTINITIALISED;
    std::shared_ptr<winrt_websocket_task> websocketTask = std::dynamic_pointer_cast<winrt_websocket_task>(websocket->task);

    std::shared_ptr<websocket_outgoing_message> msg = std::make_shared<websocket_outgoing_message>();
    msg->m_message = message;
    msg->m_asyncBlock = asyncBlock;
    msg->m_id = ++httpSingleton->m_lastId;

    if (msg->m_message.length() == 0)
    {
        return HC_E_INVALIDARG;
    }

    bool sendInProgress = false;
    {
        std::lock_guard<std::mutex> lock(websocketTask->m_outgoingMessageQueueLock);
        if (websocketTask->m_outgoingMessageQueue.size() > 0)
        {
            sendInProgress = true;
        }
        HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: send msg queue size: %d", websocketTask->m_websocketHandle->id, websocketTask->m_outgoingMessageQueue.size());

        websocketTask->m_outgoingMessageQueue.push(msg);
    }

    // No sends in progress, so start sending the message
    if (!sendInProgress)
    {
        MessageWebSocketSendMessage(websocketTask);
    }
    
    return HC_OK;
}

struct SendMessageCallbackContent
{
    std::shared_ptr<websocket_outgoing_message> nextMessage;
    std::shared_ptr<winrt_websocket_task> websocketTask;
};

HRESULT WebsockSendMessageDoWork(
    _In_opt_ void* executionRoutineContext,
    _In_ AsyncBlock* asyncBlock
    )
try
{
    auto sendMsgContext = shared_ptr_cache::fetch<SendMessageCallbackContent>(executionRoutineContext, false);
    if (sendMsgContext == nullptr)
    {
        HC_TRACE_ERROR(WEBSOCKET, "Websocket: Send message execute null");
        return E_INVALIDARG;
    }

    auto websocketTask = sendMsgContext->websocketTask;
    auto websocket = websocketTask->m_websocketHandle;
    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Send message executing", websocket->id);

    auto msg = sendMsgContext->nextMessage;
    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Message [ID %llu] [%s]", websocket->id, msg->m_id, msg->m_message.c_str());

    websocketTask->m_messageWebSocket->Control->MessageType = SocketMessageType::Utf8;
    unsigned char* uchar = reinterpret_cast<unsigned char*>(const_cast<char*>(msg->m_message.c_str()));
    websocketTask->m_messageDataWriter->WriteBytes(Platform::ArrayReference<unsigned char>(uchar, static_cast<unsigned int>(msg->m_message.length())));

    msg->m_storeAsyncOp = websocketTask->m_messageDataWriter->StoreAsync();

    msg->m_storeAsyncOp->Completed = ref new AsyncOperationCompletedHandler<unsigned int>(
        [websocketTask, msg, asyncBlock](Windows::Foundation::IAsyncOperation<unsigned int>^ asyncOp, Windows::Foundation::AsyncStatus status)
    {
        try
        {
            msg->m_storeAsyncOpStatus = status;
            unsigned int result = asyncOp->GetResults();
            HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: Message [ID %llu] send complete = %d", websocketTask->m_websocketHandle->id, msg->m_id, result);
            msg->m_storeAsyncResult = result;
        }
        catch (Platform::Exception^ ex)
        {
            msg->m_storeAsyncResult = ex->HResult;
        }
        catch (...)
        {
            msg->m_storeAsyncResult = E_FAIL;
        }

        if (FAILED(msg->m_storeAsyncResult))
        {
            HC_TRACE_ERROR(WEBSOCKET, "Websocket [ID %llu]: Message [ID %llu] send failed = 0x%0.8x", websocketTask->m_websocketHandle->id, msg->m_id, msg->m_storeAsyncResult);
        }
        CompleteAsync(asyncBlock, msg->m_storeAsyncResult, sizeof(WebSocketCompletionResult));
        MessageWebSocketSendMessage(websocketTask);
    });

    return E_PENDING;
}
CATCH_RETURN()

HRESULT WebsockSendMessageGetResult(_Inout_ AsyncProviderData* data)
{
    if (data->context == nullptr ||
        data->bufferSize < sizeof(WebSocketCompletionResult))
    {
        return E_INVALIDARG;
    }

    auto sendMsgContext = shared_ptr_cache::fetch<SendMessageCallbackContent>(data->context, false);
    if (sendMsgContext == nullptr)
    {
        HC_TRACE_ERROR(WEBSOCKET, "Websocket GetResult null");
        return E_INVALIDARG;
    }

    auto msg = sendMsgContext->nextMessage;
    auto websocket = sendMsgContext->websocketTask->m_websocketHandle;

    if (websocket == nullptr)
    {
        return E_INVALIDARG;
    }

    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: GetResult ", websocket->id);

    WebSocketCompletionResult result = {};
    result.websocket = websocket;
    result.errorCode = (FAILED(msg->m_storeAsyncResult)) ? HC_E_FAIL : HC_OK;
    result.platformErrorCode = msg->m_storeAsyncResult;
    CopyMemory(data->buffer, &result, sizeof(WebSocketCompletionResult));

    return S_OK;
}

void MessageWebSocketSendMessage(
    _In_ std::shared_ptr<winrt_websocket_task> websocketTask
    )
{
    std::shared_ptr<websocket_outgoing_message> msg;

    {
        std::lock_guard<std::mutex> lock(websocketTask->m_outgoingMessageQueueLock);
        if (websocketTask->m_outgoingMessageQueue.size() > 0)
        {
            msg = websocketTask->m_outgoingMessageQueue.front();
            websocketTask->m_outgoingMessageQueue.pop();
        }
    }
    if (msg == nullptr)
    {
        return;
    }

    std::shared_ptr<SendMessageCallbackContent> callbackContext = std::make_shared<SendMessageCallbackContent>();
    callbackContext->nextMessage = msg;
    callbackContext->websocketTask = websocketTask;
    void* rawMsg = shared_ptr_cache::store<SendMessageCallbackContent>(callbackContext);

    HC_RESULT hr = HRESULTtoHC(BeginAsync(msg->m_asyncBlock, rawMsg, HCWebSocketSendMessage, __FUNCTION__,
        [](_In_ AsyncOp op, _Inout_ AsyncProviderData* data)
    {
        switch (op)
        {
            case AsyncOp_DoWork: return WebsockSendMessageDoWork(data->context, data->async);
            case AsyncOp_GetResult: return WebsockSendMessageGetResult(data);
            case AsyncOp_Cleanup: shared_ptr_cache::fetch<SendMessageCallbackContent>(data->context, true);
        }

        return S_OK;
    }));

    if (hr == HC_OK)
    {
        hr = HRESULTtoHC(ScheduleAsync(msg->m_asyncBlock, 0));
    }
}


HC_RESULT Internal_HCWebSocketDisconnect(
    _In_ HC_WEBSOCKET_HANDLE websocket,
    _In_ HC_WEBSOCKET_CLOSE_STATUS closeStatus
    )
{
    if (websocket == nullptr)
    {
        return HC_E_INVALIDARG;
    }

    std::shared_ptr<winrt_websocket_task> websocketTask = std::dynamic_pointer_cast<winrt_websocket_task>(websocket->task);
    if (websocketTask == nullptr || websocketTask->m_messageWebSocket == nullptr)
    {
        return HC_E_NOTINITIALISED;
    }

    HC_TRACE_INFORMATION(WEBSOCKET, "Websocket [ID %llu]: disconnecting", websocket->id);
    websocketTask->m_messageWebSocket->Close(static_cast<unsigned short>(closeStatus), nullptr);
    return HC_OK;
}

