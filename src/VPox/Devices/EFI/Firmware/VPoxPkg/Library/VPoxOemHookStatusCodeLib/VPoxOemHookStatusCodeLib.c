/* $Id: VPoxOemHookStatusCodeLib.c $ */
/** @file
 * DxeVPoxOemHookStatusCodeLib.c - Logging.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <Library/ReportStatusCodeLib.h>
#include <Library/OemHookStatusCodeLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Guid/StatusCodeDataTypeId.h>
#include <Guid/StatusCodeDataTypeDebug.h>
#if 0 /* See VPoxSecExtractDebugInfo */
# include <DebugInfo.h>
#endif

#include "VPoxDebugLib.h"
#include "DevEFI.h"



EFI_STATUS EFIAPI
OemHookStatusCodeInitialize(VOID)
{
    VPoxPrintString("OemHookStatusCodeInitialize\n");
    return EFI_SUCCESS;
}


#if 0 /* vvl: With thbe new version the API changed a bit and VA_LIST isn't used any more. Before applying
       *      any changes here I would like to understand in which cases we need this help function.
       * bird: Some components sent information in this format. Search for the UUID or EFI_DEBUG_INFO usage.
       */
/**
 * Helper VPoxSecPeiReportStatusCode uses for catching some odd reports.
 */
static BOOLEAN
VPoxSecExtractDebugInfo(IN CONST EFI_STATUS_CODE_DATA *pData,
                        OUT UINT32 *puErrorLevel,
                        OUT VA_LIST *pVa,
                        OUT CHAR8 **ppszFormat)
{
    EFI_DEBUG_INFO *pDebugInfo;

    if (    !CompareGuid(&pData->Type, &gEfiStatusCodeSpecificDataGuid)
        ||  pData->HeaderSize != sizeof(*pData)
        ||  pData->Size <= sizeof(UINT64) * 12 + sizeof(EFI_DEBUG_INFO) + 1)
        return FALSE;

    pDebugInfo  = (EFI_DEBUG_INFO *)(pData + 1);
    *pVa        = (VA_LIST)(pDebugInfo + 1);
    *ppszFormat = (CHAR8 *)((UINT64 *)pVa + 12);
    return TRUE;
}
#endif

/** Worker that dumps the raw data. */
static void
VPoxOemHookStatusCodeReportRawDump(EFI_STATUS_CODE_TYPE Type,
                                   EFI_STATUS_CODE_VALUE Value,
                                   UINT32 Instance,
                                   CONST EFI_GUID *CallerId)
{
    VPoxPrintString("Report: Type=");
    VPoxPrintHex(Type, sizeof(Type));
    VPoxPrintString(" Value=");
    VPoxPrintHex(Value, sizeof(Value));

    VPoxPrintString(" Instance=");
    VPoxPrintHex(Instance, sizeof(Instance));
    if (CallerId)
    {
        VPoxPrintString(" CallerId=");
        VPoxPrintGuid(CallerId);
    }

#define CASE_PRINT(Head,Print,Tail) \
        case Head ## Print ## Tail : VPoxPrintString(" " #Print); break
    switch (Type & EFI_STATUS_CODE_SEVERITY_MASK) /* quick guess work... */
    {
        CASE_PRINT(EFI_ERROR_,MINOR,);
        CASE_PRINT(EFI_ERROR_,MAJOR,);
        CASE_PRINT(EFI_ERROR_,UNRECOVERED,);
        CASE_PRINT(EFI_ERROR_,UNCONTAINED,);
    }
    switch (Type & EFI_STATUS_CODE_TYPE_MASK) /* quick guess work... */
    {
        CASE_PRINT(EFI_,PROGRESS,_CODE);
        CASE_PRINT(EFI_,ERROR,_CODE);
        CASE_PRINT(EFI_,DEBUG,_CODE);
    }
#undef CASE_PRINT
    VPoxPrintChar('\n');
}


EFI_STATUS EFIAPI
OemHookStatusCodeReport(IN EFI_STATUS_CODE_TYPE Type,
                        IN EFI_STATUS_CODE_VALUE Value,
                        IN UINT32 Instance,
                        IN EFI_GUID *CallerId OPTIONAL,
                        IN EFI_STATUS_CODE_DATA *Data OPTIONAL)
{
    /*
     * Try figure out the data payload
     */
    if (Data != NULL)
    {
        CHAR8      *pszFilename;
        CHAR8      *pszDescription;
        UINT32      uLine;
        UINT32      uErrorLevel;
        BASE_LIST   bs;
        CHAR8      *pszFormat;

        if (ReportStatusCodeExtractAssertInfo(Type, Value, Data, &pszFilename,
                                              &pszDescription, &uLine))
        {
            VPoxPrintString("Assertion Failed! Line=0x");
            VPoxPrintHex(uLine, sizeof(uLine));
            if (pszFilename)
            {
                VPoxPrintString(" File=");
                VPoxPrintString(pszFilename);
            }
            if (pszDescription)
            {
                VPoxPrintString(" Desc=");
                VPoxPrintString(pszDescription);
            }
            VPoxPrintChar('\n');
        }
        else if (   ReportStatusCodeExtractDebugInfo(Data, &uErrorLevel, &bs, &pszFormat)
#if 0 /* See question at VPoxSecExtractDebugInfo. */
                 || VPoxSecExtractDebugInfo(Data, &uErrorLevel, &va, &pszFormat)
#endif
        )
        {
            CHAR8   szBuf[128];
            UINTN   cch;

            cch = AsciiBSPrint(szBuf, sizeof(szBuf), pszFormat, bs);
            if (cch >= sizeof(szBuf))
                cch = sizeof(szBuf) - 1;
            while (     cch > 0
                   &&   (   szBuf[cch - 1] == '\n'
                         || szBuf[cch - 1] == '\r'))
                cch--;
            szBuf[cch] = '\0';

            VPoxPrintString("DBG/");
            VPoxPrintHex(uErrorLevel, sizeof(uErrorLevel));
            VPoxPrintString(": ");
            VPoxPrintString(szBuf);
            VPoxPrintChar('\n');
        }
        else
        {
            /*
             * Unknown data, resort to raw dump of everything.
             */
            VPoxOemHookStatusCodeReportRawDump(Type, Value, Instance, CallerId);

            VPoxPrintString("OemReport: Unknown data type ");
            VPoxPrintGuid(&Data->Type);
            VPoxPrintString(" (Size=");
            VPoxPrintHex(Data->Size, sizeof(Data->Size));
            VPoxPrintString(" HeaderSize=");
            VPoxPrintHex(Data->HeaderSize, sizeof(Data->HeaderSize));
            VPoxPrintString(")\n");
            if (Data->Size > 0 && Data->Size <= 128)
                VPoxPrintHexDump(Data + 1, Data->Size);
        }
    }
    /*
     * No data, do a raw dump.
     */
    else
        VPoxOemHookStatusCodeReportRawDump(Type, Value, Instance, CallerId);

    return EFI_SUCCESS;
}

