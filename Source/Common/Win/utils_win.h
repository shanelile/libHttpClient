// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#include "../Global/mem.h"

using http_internal_wstring = http_internal_basic_string<wchar_t>;

http_internal_string utf8_from_utf16(const http_internal_wstring& utf16);
http_internal_wstring utf16_from_utf8(const http_internal_string& utf8);

http_internal_string utf8_from_utf16(_In_z_ PCWSTR utf16);
http_internal_wstring utf16_from_utf8(_In_z_ UTF8CSTR utf8);

http_internal_string utf8_from_utf16(_In_reads_(size) PCWSTR utf16, size_t size);
http_internal_wstring utf16_from_utf8(_In_reads_(size) UTF8CSTR utf8, size_t size);
