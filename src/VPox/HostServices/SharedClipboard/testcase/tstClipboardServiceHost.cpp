/* $Id: tstClipboardServiceHost.cpp $ */
/** @file
 * Shared Clipboard host service test case.
 */

/*
 * Copyright (C) 2011-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "../VPoxSharedClipboardSvc-internal.h"

#include <VPox/HostServices/VPoxClipboardSvc.h>
#ifdef RT_OS_WINDOWS
# include <VPox/GuestHost/SharedClipboard-win.h>
#endif

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/test.h>

extern "C" DECLCALLBACK(DECLEXPORT(int)) VPoxHGCMSvcLoad (VPOXHGCMSVCFNTABLE *ptable);

static SHCLCLIENT g_Client;
static VPOXHGCMSVCHELPERS g_Helpers = { NULL };

/** Simple call handle structure for the guest call completion callback */
struct VPOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code */
    int32_t rc;
};

/** Call completion callback for guest calls. */
static DECLCALLBACK(int) callComplete(VPOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    callHandle->rc = rc;
    return VINF_SUCCESS;
}

static int setupTable(VPOXHGCMSVCFNTABLE *pTable)
{
    pTable->cbSize = sizeof(*pTable);
    pTable->u32Version = VPOX_HGCM_SVC_VERSION;
    g_Helpers.pfnCallComplete = callComplete;
    pTable->pHelpers = &g_Helpers;
    return VPoxHGCMSvcLoad(pTable);
}

static void testSetMode(void)
{
    struct VPOXHGCMSVCPARM parms[2];
    VPOXHGCMSVCFNTABLE table;
    uint32_t u32Mode;
    int rc;

    RTTestISub("Testing VPOX_SHCL_HOST_FN_SET_MODE");
    rc = setupTable(&table);
    RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("rc=%Rrc\n", rc));

    /* Reset global variable which doesn't reset itself. */
    HGCMSvcSetU32(&parms[0], VPOX_SHCL_MODE_OFF);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    u32Mode = ShClSvcGetMode();
    RTTESTI_CHECK_MSG(u32Mode == VPOX_SHCL_MODE_OFF, ("u32Mode=%u\n", (unsigned) u32Mode));

    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    HGCMSvcSetU64(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    HGCMSvcSetU32(&parms[0], VPOX_SHCL_MODE_HOST_TO_GUEST);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    u32Mode = ShClSvcGetMode();
    RTTESTI_CHECK_MSG(u32Mode == VPOX_SHCL_MODE_HOST_TO_GUEST, ("u32Mode=%u\n", (unsigned) u32Mode));

    HGCMSvcSetU32(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_NOT_SUPPORTED);

    u32Mode = ShClSvcGetMode();
    RTTESTI_CHECK_MSG(u32Mode == VPOX_SHCL_MODE_OFF, ("u32Mode=%u\n", (unsigned) u32Mode));
    table.pfnUnload(NULL);
}

#ifdef VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS
static void testSetTransferMode(void)
{
    struct VPOXHGCMSVCPARM parms[2];
    VPOXHGCMSVCFNTABLE table;

    RTTestISub("Testing VPOX_SHCL_HOST_FN_SET_TRANSFER_MODE");
    int rc = setupTable(&table);
    RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("rc=%Rrc\n", rc));

    /* Invalid parameter. */
    HGCMSvcSetU64(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);

    /* Invalid mode. */
    HGCMSvcSetU32(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_FLAGS);

    /* Enable transfers. */
    HGCMSvcSetU32(&parms[0], VPOX_SHCL_TRANSFER_MODE_ENABLED);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);

    /* Disable transfers again. */
    HGCMSvcSetU32(&parms[0], VPOX_SHCL_TRANSFER_MODE_DISABLED);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_TRANSFER_MODE, 1, parms);
    RTTESTI_CHECK_RC(rc, VINF_SUCCESS);
}
#endif /* VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

/* Adds a host data read request message to the client's message queue. */
static void testMsgAddReadData(PSHCLCLIENT pClient, SHCLFORMATS fFormats)
{
    int rc = ShClSvcDataReadRequest(pClient, fFormats, NULL /* pidEvent */);
    RTTESTI_CHECK_RC_OK(rc);
}

/* Does testing of VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, needed for providing compatibility to older Guest Additions clients. */
static void testGetHostMsgOld(void)
{
    struct VPOXHGCMSVCPARM parms[2];
    VPOXHGCMSVCFNTABLE table;
    VPOXHGCMCALLHANDLE_TYPEDEF call;
    int rc;

    RTTestISub("Setting up VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT test");
    rc = setupTable(&table);
    RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("rc=%Rrc\n", rc));
    /* Unless we are bidirectional the host message requests will be dropped. */
    HGCMSvcSetU32(&parms[0], VPOX_SHCL_MODE_BIDIRECTIONAL);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_MODE, 1, parms);
    RTTESTI_CHECK_RC_OK(rc);

    rc = shClSvcClientInit(&g_Client, 1 /* clientId */);
    RTTESTI_CHECK_RC_OK(rc);

    RTTestISub("Testing one format, waiting guest call.");
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This should get updated only when the guest call completes. */
    testMsgAddReadData(&g_Client, VPOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    table.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing one format, no waiting guest calls.");
    table.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VPOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    table.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, waiting guest call.");
    table.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This should get updated only when the guest call completes. */
    testMsgAddReadData(&g_Client, VPOX_SHCL_FMT_UNICODETEXT | VPOX_SHCL_FMT_HTML);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    table.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);

    RTTestISub("Testing two formats, no waiting guest calls.");
    table.pfnConnect(NULL, 1 /* clientId */, &g_Client, 0, 0);
    testMsgAddReadData(&g_Client, VPOX_SHCL_FMT_UNICODETEXT | VPOX_SHCL_FMT_HTML);
    HGCMSvcSetU32(&parms[0], 0);
    HGCMSvcSetU32(&parms[1], 0);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_UNICODETEXT);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK(parms[0].u.uint32 == VPOX_SHCL_HOST_MSG_READ_DATA);
    RTTESTI_CHECK(parms[1].u.uint32 == VPOX_SHCL_FMT_HTML);
    RTTESTI_CHECK_RC_OK(call.rc);
    call.rc = VERR_IPE_UNINITIALIZED_STATUS;
    table.pfnCall(NULL, &call, 1 /* clientId */, &g_Client, VPOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT, 2, parms, 0);
    RTTESTI_CHECK_RC(call.rc, VERR_IPE_UNINITIALIZED_STATUS);  /* This call should not complete yet. */
    table.pfnDisconnect(NULL, 1 /* clientId */, &g_Client);
    table.pfnUnload(NULL);
}

static void testSetHeadless(void)
{
    struct VPOXHGCMSVCPARM parms[2];
    VPOXHGCMSVCFNTABLE table;
    bool fHeadless;
    int rc;

    RTTestISub("Testing HOST_FN_SET_HEADLESS");
    rc = setupTable(&table);
    RTTESTI_CHECK_MSG_RETV(RT_SUCCESS(rc), ("rc=%Rrc\n", rc));
    /* Reset global variable which doesn't reset itself. */
    HGCMSvcSetU32(&parms[0], false);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == false, ("fHeadless=%RTbool\n", fHeadless));
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           0, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           2, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU64(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           1, parms);
    RTTESTI_CHECK_RC(rc, VERR_INVALID_PARAMETER);
    HGCMSvcSetU32(&parms[0], true);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
    HGCMSvcSetU32(&parms[0], 99);
    rc = table.pfnHostCall(NULL, VPOX_SHCL_HOST_FN_SET_HEADLESS,
                           1, parms);
    RTTESTI_CHECK_RC_OK(rc);
    fHeadless = ShClSvcGetHeadless();
    RTTESTI_CHECK_MSG(fHeadless == true, ("fHeadless=%RTbool\n", fHeadless));
    table.pfnUnload(NULL);
}

static void testHostCall(void)
{
    testSetMode();
#ifdef VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS
    testSetTransferMode();
#endif /* VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS */
    testSetHeadless();
}

#ifdef RT_OS_WINDOWS
# include "VPoxOrgCfHtml1.h"    /* From chrome 97.0.4692.71 */
# include "VPoxOrgMimeHtml1.h"

static void testHtmlCf(void)
{
    RTTestISub("CF_HTML");

    char    *pszOutput = NULL;
    uint32_t cbOutput  = UINT32_MAX/2;
    RTTestIDisableAssertions();
    RTTESTI_CHECK_RC(SharedClipboardWinConvertCFHTMLToMIME("", 0, &pszOutput, &cbOutput), VERR_INVALID_PARAMETER);
    RTTestIRestoreAssertions();

    pszOutput = NULL;
    cbOutput  = UINT32_MAX/2;
    RTTESTI_CHECK_RC(SharedClipboardWinConvertCFHTMLToMIME((char *)&g_abVPoxOrgCfHtml1[0], g_cbVPoxOrgCfHtml1,
                                                           &pszOutput, &cbOutput), VINF_SUCCESS);
    RTTESTI_CHECK(cbOutput == g_cbVPoxOrgMimeHtml1);
    RTTESTI_CHECK(memcmp(pszOutput, g_abVPoxOrgMimeHtml1, cbOutput) == 0);
    RTMemFree(pszOutput);


    static RTSTRTUPLE const s_aRoundTrips[] =
    {
        { RT_STR_TUPLE("") },
        { RT_STR_TUPLE("1") },
        { RT_STR_TUPLE("12") },
        { RT_STR_TUPLE("123") },
        { RT_STR_TUPLE("1234") },
        { RT_STR_TUPLE("12345") },
        { RT_STR_TUPLE("123456") },
        { RT_STR_TUPLE("1234567") },
        { RT_STR_TUPLE("12345678") },
        { RT_STR_TUPLE("123456789") },
        { RT_STR_TUPLE("1234567890") },
        { RT_STR_TUPLE("<h2>asdfkjhasdflhj</h2>") },
        { RT_STR_TUPLE("<h2>asdfkjhasdflhj</h2>\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0") },
        { (const char *)g_abVPoxOrgMimeHtml1, sizeof(g_abVPoxOrgMimeHtml1) },
    };

    for (size_t i = 0; i < RT_ELEMENTS(s_aRoundTrips); i++)
    {
        int      rc;
        char    *pszCfHtml = NULL;
        uint32_t cbCfHtml  = UINT32_MAX/2;
        rc = SharedClipboardWinConvertMIMEToCFHTML(s_aRoundTrips[i].psz, s_aRoundTrips[i].cch + 1, &pszCfHtml, &cbCfHtml);
        if (rc == VINF_SUCCESS)
        {
            if (strlen(pszCfHtml) + 1 != cbCfHtml)
                RTTestIFailed("#%u: SharedClipboardWinConvertMIMEToCFHTML(%s, %#zx,,) returned incorrect length: %#x, actual %#zx",
                              i, s_aRoundTrips[i].psz, s_aRoundTrips[i].cch, cbCfHtml, strlen(pszCfHtml) + 1);

            char     *pszHtml = NULL;
            uint32_t  cbHtml  = UINT32_MAX/4;
            rc = SharedClipboardWinConvertCFHTMLToMIME(pszCfHtml, (uint32_t)strlen(pszCfHtml), &pszHtml, &cbHtml);
            if (rc == VINF_SUCCESS)
            {
                if (strlen(pszHtml) + 1 != cbHtml)
                    RTTestIFailed("#%u: SharedClipboardWinConvertCFHTMLToMIME(%s, %#zx,,) returned incorrect length: %#x, actual %#zx",
                                  i, pszHtml, strlen(pszHtml), cbHtml, strlen(pszHtml) + 1);
                if (strcmp(pszHtml, s_aRoundTrips[i].psz) != 0)
                    RTTestIFailed("#%u: roundtrip for '%s' LB %#zx failed, ended up with '%s'",
                                  i, s_aRoundTrips[i].psz, s_aRoundTrips[i].cch, pszHtml);
                RTMemFree(pszHtml);
            }
            else
                RTTestIFailed("#%u: SharedClipboardWinConvertCFHTMLToMIME(%s, %#zx,,) returned %Rrc, expected VINF_SUCCESS",
                              i, pszCfHtml, strlen(pszCfHtml), rc);
            RTMemFree(pszCfHtml);
        }
        else
            RTTestIFailed("#%u: SharedClipboardWinConvertMIMEToCFHTML(%s, %#zx,,) returned %Rrc, expected VINF_SUCCESS",
                          i, s_aRoundTrips[i].psz, s_aRoundTrips[i].cch, rc);
    }
}

#endif /* RT_OS_WINDOWS */

int main(int argc, char *argv[])
{
    /*
     * Init the runtime, test and say hello.
     */
    const char *pcszExecName;
    NOREF(argc);
    pcszExecName = strrchr(argv[0], '/');
    pcszExecName = pcszExecName ? pcszExecName + 1 : argv[0];
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate(pcszExecName, &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    /* Don't let assertions in the host service panic (core dump) the test cases. */
    RTAssertSetMayPanic(false);

    /*
     * Run the tests.
     */
    testHostCall();
    testGetHostMsgOld();
#ifdef RT_OS_WINDOWS
    testHtmlCf();
#endif

    /*
     * Summary
     */
    return RTTestSummaryAndDestroy(hTest);
}

int ShClSvcImplInit(VPOXHGCMSVCFNTABLE *) { return VINF_SUCCESS; }
void ShClSvcImplDestroy() { }
int ShClSvcImplDisconnect(PSHCLCLIENT) { return VINF_SUCCESS; }
int ShClSvcImplConnect(PSHCLCLIENT, bool) { return VINF_SUCCESS; }
int ShClSvcImplFormatAnnounce(PSHCLCLIENT, SHCLFORMATS) { AssertFailed(); return VINF_SUCCESS; }
int ShClSvcImplReadData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT, void *, uint32_t, unsigned int *) { AssertFailed(); return VERR_WRONG_ORDER; }
int ShClSvcImplWriteData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT, void *, uint32_t) { AssertFailed(); return VINF_SUCCESS; }
int ShClSvcImplSync(PSHCLCLIENT) { return VINF_SUCCESS; }

#ifdef VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS
int ShClSvcImplTransferCreate(PSHCLCLIENT, PSHCLTRANSFER) { return VINF_SUCCESS; }
int ShClSvcImplTransferDestroy(PSHCLCLIENT, PSHCLTRANSFER) { return VINF_SUCCESS; }
int ShClSvcImplTransferGetRoots(PSHCLCLIENT, PSHCLTRANSFER) { return VINF_SUCCESS; }
#endif /* VPOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

