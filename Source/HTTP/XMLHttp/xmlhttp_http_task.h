// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once
#include "pch.h"
#include <wrl.h>
#if !HC_XDK_API
#include <msxml6.h>
#else
#include <ixmlhttprequest2.h>
#endif
#include "http_buffer.h"
#include "utils.h"

class xmlhttp_http_task : public xbox::httpclient::hc_task
{
public:
    xmlhttp_http_task(
        _In_ HC_CALL_HANDLE call,
        _In_ AsyncBlock* asyncBlock
        );
    ~xmlhttp_http_task();

    void perform_async(
        _In_ HC_CALL_HANDLE call,
        _In_ AsyncBlock* asyncBlock
        );

    void set_status_code(_In_ uint32_t statusCode);
    uint32_t get_status_code();
    http_internal_vector<http_internal_string> split(
        _In_ const http_internal_string& s,
        _In_z_ const char* delim);
    void set_headers(_In_ WCHAR* allResponseHeaders);
    const http_internal_vector<http_internal_string>& get_headers_names();
    const http_internal_vector<http_internal_string>& get_headers_values();
    bool has_error();
    void set_exception(const std::exception_ptr& exceptionPtr);
    http_buffer& response_buffer();
    HC_CALL_HANDLE call();
    AsyncBlock* async_block();

private:
    HC_CALL_HANDLE m_call;
    AsyncBlock* m_asyncBlock;
    http_buffer m_responseBuffer;
    uint32_t m_statusCode;
    std::exception_ptr m_exceptionPtr;
    HRESULT m_hrCoInit;
    http_internal_vector<http_internal_string> m_headerNames;
    http_internal_vector<http_internal_string> m_headerValues;
    Microsoft::WRL::ComPtr<IXMLHTTPRequest2> m_hRequest;
    Microsoft::WRL::ComPtr<ISequentialStream> m_hRequestBodyStream;
};

