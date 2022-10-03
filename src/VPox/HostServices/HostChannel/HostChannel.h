/* @file
 *
 * Host Channel
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VPOX_INCLUDED_SRC_HostChannel_HostChannel_h
#define VPOX_INCLUDED_SRC_HostChannel_HostChannel_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/list.h>

#define LOG_GROUP LOG_GROUP_HGCM
#include <VPox/log.h>
#include <VPox/HostServices/VPoxHostChannel.h>

#define HOSTCHLOG Log

#ifdef DEBUG_sunlover
# undef HOSTCHLOG
# define HOSTCHLOG LogRel
#endif /* DEBUG_sunlover */

struct VPOXHOSTCHCTX;
typedef struct VPOXHOSTCHCTX VPOXHOSTCHCTX;

typedef struct VPOXHOSTCHCLIENT
{
    RTLISTNODE nodeClient;

    VPOXHOSTCHCTX *pCtx;

    uint32_t u32ClientID;

    RTLISTANCHOR listChannels;
    uint32_t volatile u32HandleSrc;

    RTLISTANCHOR listContexts; /* Callback contexts. */

    RTLISTANCHOR listEvents;

    bool fAsync;        /* Guest is waiting for a message. */

    struct {
        VPOXHGCMCALLHANDLE callHandle;
        VPOXHGCMSVCPARM *paParms;
    } async;

} VPOXHOSTCHCLIENT;


/*
 * The service functions. Locking is between the service thread and the host channel provider thread.
 */
int vpoxHostChannelLock(void);
void vpoxHostChannelUnlock(void);

int vpoxHostChannelInit(void);
void vpoxHostChannelDestroy(void);

int vpoxHostChannelClientConnect(VPOXHOSTCHCLIENT *pClient);
void vpoxHostChannelClientDisconnect(VPOXHOSTCHCLIENT *pClient);

int vpoxHostChannelAttach(VPOXHOSTCHCLIENT *pClient,
                          uint32_t *pu32Handle,
                          const char *pszName,
                          uint32_t u32Flags);
int vpoxHostChannelDetach(VPOXHOSTCHCLIENT *pClient,
                          uint32_t u32Handle);

int vpoxHostChannelSend(VPOXHOSTCHCLIENT *pClient,
                        uint32_t u32Handle,
                        const void *pvData,
                        uint32_t cbData);
int vpoxHostChannelRecv(VPOXHOSTCHCLIENT *pClient,
                        uint32_t u32Handle,
                        void *pvData,
                        uint32_t cbData,
                        uint32_t *pu32DataReceived,
                        uint32_t *pu32DataRemaining);
int vpoxHostChannelControl(VPOXHOSTCHCLIENT *pClient,
                           uint32_t u32Handle,
                           uint32_t u32Code,
                           void *pvParm,
                           uint32_t cbParm,
                           void *pvData,
                           uint32_t cbData,
                           uint32_t *pu32SizeDataReturned);

int vpoxHostChannelEventWait(VPOXHOSTCHCLIENT *pClient,
                             bool *pfEvent,
                             VPOXHGCMCALLHANDLE callHandle,
                             VPOXHGCMSVCPARM *paParms);

int vpoxHostChannelEventCancel(VPOXHOSTCHCLIENT *pClient);

int vpoxHostChannelQuery(VPOXHOSTCHCLIENT *pClient,
                         const char *pszName,
                         uint32_t u32Code,
                         void *pvParm,
                         uint32_t cbParm,
                         void *pvData,
                         uint32_t cbData,
                         uint32_t *pu32SizeDataReturned);

int vpoxHostChannelRegister(const char *pszName,
                            const VPOXHOSTCHANNELINTERFACE *pInterface,
                            uint32_t cbInterface);
int vpoxHostChannelUnregister(const char *pszName);


void vpoxHostChannelEventParmsSet(VPOXHGCMSVCPARM *paParms,
                                  uint32_t u32ChannelHandle,
                                  uint32_t u32Id,
                                  const void *pvEvent,
                                  uint32_t cbEvent);

void vpoxHostChannelReportAsync(VPOXHOSTCHCLIENT *pClient,
                                uint32_t u32ChannelHandle,
                                uint32_t u32Id,
                                const void *pvEvent,
                                uint32_t cbEvent);

#endif /* !VPOX_INCLUDED_SRC_HostChannel_HostChannel_h */
