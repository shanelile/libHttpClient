// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once
// selected MS SAL annotations

#ifndef _Printf_format_string_
#define _Printf_format_string_ 
#endif

#ifndef _Post_invalid_
#define _Post_invalid_ 
#endif

#ifndef _In_opt_
#define _In_opt_ 
#endif

#ifndef _In_z_
#define _In_z_ 
#endif

#ifndef _In_opt_z_
#define _In_opt_z_ 
#endif

#ifndef _In_reads_bytes_
#define _In_reads_bytes_(size) 
#endif

#ifndef _In_reads_
#define _In_reads_(size) 
#endif

#ifndef _In_reads_bytes_opt_
#define _In_reads_bytes_opt_(size) 
#endif

#ifndef _Outptr_result_bytebuffer_maybenull_
#define _Outptr_result_bytebuffer_maybenull_(size)
#endif

#ifndef __cdecl
#define __cdecl 
#endif

#ifndef _Out_
#define _Out_ 
#endif

#ifndef _Out_opt_
#define _Out_opt_ 
#endif

#ifndef _Out_writes_to_
#define _Out_writes_to_(bytes, buffer)
#endif

#ifndef _Out_writes_to_opt_
#define _Out_writes_to_opt_(buffersize, size)
#endif

#ifndef _Outptr_
#define _Outptr_ 
#endif

#ifndef _Inout_
#define _Inout_ 
#endif

#ifndef _Field_z_
#define _Field_z_ 
#endif

#ifndef _Field_size_
#define _Field_size_(size) 
#endif

#ifndef _Field_size_bytes_
#define _Field_size_bytes_(size) 
#endif

#ifndef __analysis_assume
#define __analysis_assume(condition)
#endif

// TODO: Add check for android
#ifndef CONST
#define CONST const
#endif

typedef unsigned char BYTE;
typedef char CHAR;
typedef wchar_t WCHAR;

typedef CONST CHAR *PCSTR;
typedef BYTE *PBYTE;
typedef CONST WCHAR *PCWSTR;