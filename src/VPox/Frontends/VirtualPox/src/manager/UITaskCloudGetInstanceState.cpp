/* $Id: UITaskCloudGetInstanceState.cpp $ */
/** @file
 * VPox Qt GUI - UITaskCloudGetInstanceState class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualPox Open Source Edition (OSE), as
 * available from http://www.virtualpox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualPox OSE distribution. VirtualPox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UICloudNetworkingStuff.h"
#include "UITaskCloudGetInstanceState.h"


UITaskCloudGetInstanceState::UITaskCloudGetInstanceState(const CCloudClient &comCloudClient, const QString &strId)
    : UITask(Type_CloudGetInstanceState)
    , m_comCloudClient(comCloudClient)
    , m_strId(strId)
{
}

QString UITaskCloudGetInstanceState::result() const
{
    m_mutex.lock();
    const QString strResult = m_strResult;
    m_mutex.unlock();
    return strResult;
}

CVirtualPoxErrorInfo UITaskCloudGetInstanceState::errorInfo()
{
    m_mutex.lock();
    CVirtualPoxErrorInfo comErrorInfo = m_comErrorInfo;
    m_mutex.unlock();
    return comErrorInfo;
}

void UITaskCloudGetInstanceState::run()
{
    m_mutex.lock();
    m_strResult = getInstanceInfo(KVirtualSystemDescriptionType_CloudInstanceState, m_comCloudClient, m_strId);
    m_mutex.unlock();
}
