/** @file
 * IPRT - Base64, MIME content transfer encoding.
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualPox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef IPRT_INCLUDED_base64_h
#define IPRT_INCLUDED_base64_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_base64     RTBase64 - Base64, MIME content transfer encoding.
 * @ingroup grp_rt
 * @{
 */

/** @def RTBASE64_EOL_SIZE
 * The size of the end-of-line marker. */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define RTBASE64_EOL_SIZE      (sizeof("\r\n") - 1)
#else
# define RTBASE64_EOL_SIZE      (sizeof("\n")   - 1)
#endif


/** @name Flags for RTBase64EncodeEx() and RTBase64EncodedLengthEx().
 * @{ */
/** Insert line breaks into encoded string.
 * The size of the end-of-line marker is that that of the host platform.
 */
#define RTBASE64_FLAGS_EOL_NATIVE       UINT32_C(0) /**< Use native newlines. */
#define RTBASE64_FLAGS_NO_LINE_BREAKS   UINT32_C(1) /**< No newlines.  */
#define RTBASE64_FLAGS_EOL_LF           UINT32_C(2) /**< Use UNIX-style newlines. */
#define RTBASE64_FLAGS_EOL_CRLF         UINT32_C(3) /**< Use DOS-style newlines. */
#define RTBASE64_FLAGS_EOL_STYLE_MASK   UINT32_C(3) /**< End-of-line style mask. */
/** @} */


/**
 * Calculates the decoded data size for a Base64 encoded string.
 *
 * @returns The length in bytes. -1 if the encoding is bad.
 *
 * @param   pszString       The Base64 encoded string.
 * @param   ppszEnd         If not NULL, this will point to the first char
 *                          following the Base64 encoded text block. If
 *                          NULL the entire string is assumed to be Base64.
 */
RTDECL(ssize_t) RTBase64DecodedSize(const char *pszString, char **ppszEnd);

/**
 * Calculates the decoded data size for a Base64 encoded string.
 *
 * @returns The length in bytes. -1 if the encoding is bad.
 *
 * @param   pszString       The Base64 encoded string.
 * @param   cchStringMax    The max length to decode, use RTSTR_MAX if the
 *                          length of @a pszString is not known and it is
 *                          really zero terminated.
 * @param   ppszEnd         If not NULL, this will point to the first char
 *                          following the Base64 encoded text block. If
 *                          NULL the entire string is assumed to be Base64.
 */
RTDECL(ssize_t) RTBase64DecodedSizeEx(const char *pszString, size_t cchStringMax, char **ppszEnd);

/**
 * Decodes a Base64 encoded string into the buffer supplied by the caller.
 *
 * @returns IPRT status code.
 * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small. pcbActual will not
 *          be set, nor will ppszEnd.
 * @retval  VERR_INVALID_BASE64_ENCODING if the encoding is wrong.
 *
 * @param   pszString       The Base64 string. Whether the entire string or
 *                          just the start of the string is in Base64 depends
 *                          on whether ppszEnd is specified or not.
 * @param   pvData          Where to store the decoded data.
 * @param   cbData          The size of the output buffer that pvData points to.
 * @param   pcbActual       Where to store the actual number of bytes returned.
 *                          Optional.
 * @param   ppszEnd         Indicates that the string may contain other stuff
 *                          after the Base64 encoded data when not NULL. Will
 *                          be set to point to the first char that's not part of
 *                          the encoding. If NULL the entire string must be part
 *                          of the Base64 encoded data.
 */
RTDECL(int) RTBase64Decode(const char *pszString, void *pvData, size_t cbData, size_t *pcbActual, char **ppszEnd);

/**
 * Decodes a Base64 encoded string into the buffer supplied by the caller.
 *
 * @returns IPRT status code.
 * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small. pcbActual will not
 *          be set, nor will ppszEnd.
 * @retval  VERR_INVALID_BASE64_ENCODING if the encoding is wrong.
 *
 * @param   pszString       The Base64 string. Whether the entire string or
 *                          just the start of the string is in Base64 depends
 *                          on whether ppszEnd is specified or not.
 * @param   cchStringMax    The max length to decode, use RTSTR_MAX if the
 *                          length of @a pszString is not known and it is
 *                          really zero terminated.
 * @param   pvData          Where to store the decoded data.
 * @param   cbData          The size of the output buffer that pvData points to.
 * @param   pcbActual       Where to store the actual number of bytes returned.
 *                          Optional.
 * @param   ppszEnd         Indicates that the string may contain other stuff
 *                          after the Base64 encoded data when not NULL. Will
 *                          be set to point to the first char that's not part of
 *                          the encoding. If NULL the entire string must be part
 *                          of the Base64 encoded data.
 */
RTDECL(int) RTBase64DecodeEx(const char *pszString, size_t cchStringMax, void *pvData, size_t cbData,
                             size_t *pcbActual, char **ppszEnd);


/**
 * Calculates the length of the Base64 encoding of a given number of bytes of
 * data produced by RTBase64Encode().
 *
 * @returns The Base64 string length.
 * @param   cbData      The number of bytes to encode.
 */
RTDECL(size_t) RTBase64EncodedLength(size_t cbData);

/**
 * Calculates the length of the Base64 encoding of a given number of bytes of
 * data produced by RTBase64EncodeEx() with the same @a fFlags.
 *
 * @returns The Base64 string length.
 * @param   cbData      The number of bytes to encode.
 * @param   fFlags      Flags, any combination of the RTBASE64_FLAGS \#defines.
 */
RTDECL(size_t) RTBase64EncodedLengthEx(size_t cbData, uint32_t fFlags);

/**
 * Encodes the specifed data into a Base64 string, the caller supplies the
 * output buffer.
 *
 * This is equivalent to calling RTBase64EncodeEx() with no flags.
 *
 * @returns IRPT status code.
 * @retval  VERR_BUFFER_OVERFLOW if the output buffer is too small. The buffer
 *          may contain an invalid Base64 string.
 *
 * @param   pvData      The data to encode.
 * @param   cbData      The number of bytes to encode.
 * @param   pszBuf      Where to put the Base64 string.
 * @param   cbBuf       The size of the output buffer, including the terminator.
 * @param   pcchActual  The actual number of characters returned.
 */
RTDECL(int) RTBase64Encode(const void *pvData, size_t cbData, char *pszBuf, size_t cbBuf, size_t *pcchActual);

/**
 * Encodes the specifed data into a Base64 string, the caller supplies the
 * output buffer.
 *
 * @returns IRPT status code.
 * @retval  VERR_BUFFER_OVERFLOW if the output buffer is too small. The buffer
 *          may contain an invalid Base64 string.
 *
 * @param   pvData      The data to encode.
 * @param   cbData      The number of bytes to encode.
 * @param   fFlags      Flags, any combination of the RTBASE64_FLAGS \#defines.
 * @param   pszBuf      Where to put the Base64 string.
 * @param   cbBuf       The size of the output buffer, including the terminator.
 * @param   pcchActual  The actual number of characters returned.
 */
RTDECL(int) RTBase64EncodeEx(const void *pvData, size_t cbData, uint32_t fFlags,
                             char *pszBuf, size_t cbBuf, size_t *pcchActual);

/** @}  */

RT_C_DECLS_END

#endif /* !IPRT_INCLUDED_base64_h */

