/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description:
*
*/

/**
 @file
 @internalComponent
*/

#include "refppnotificationman.h"
#include <usb/usblogger.h>
#include <usb/hostms/policypluginnotifier.hrh>
#include "srvpanic.h"

 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmRefPP");
#endif

#ifdef __OVER_DUMMYCOMPONENT__
const TUid KMountPolicyNotifierUid = {0x1028653E};
#else
const TUid KMountPolicyNotifierUid = {KUidMountPolicyNotifier};
#endif

CMsmmPolicyNotificationManager::~CMsmmPolicyNotificationManager()
    {
    LOG_FUNC
    Cancel();
    iErrorQueue.Close();
    iNotifier.Close();
    }

CMsmmPolicyNotificationManager* CMsmmPolicyNotificationManager::NewL()
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmPolicyNotificationManager* self = 
        CMsmmPolicyNotificationManager::NewLC();
    CleanupStack::Pop(self);
    
    return self;
    }

CMsmmPolicyNotificationManager* CMsmmPolicyNotificationManager::NewLC()
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmPolicyNotificationManager* self = 
        new (ELeave) CMsmmPolicyNotificationManager();
    CleanupStack::PushL(self);
    self->ConstructL();
    
    return self;
    }

void CMsmmPolicyNotificationManager::SendErrorNotificationL(
        const THostMsErrData& aErrData)
    {
    LOG_FUNC

    // Print error notification data to log
    LOGTEXT2(_L("Err:iError = %d"), aErrData.iError);
    LOGTEXT2(_L("Err:iE32Error = %d"), aErrData.iE32Error);
    LOGTEXT2(_L("Err:iDriveName = %d"), aErrData.iDriveName);
    LOGTEXT2(_L("Err:iManufacturerString = %S"), &aErrData.iManufacturerString);
    LOGTEXT2(_L("Err:iProductString = %S"), &aErrData.iProductString);
    
    THostMsErrorDataPckg errPckg = aErrData;
    iErrorQueue.AppendL(errPckg);
    if (!IsActive())
    	{
    	SendNotification();
    	}
    }

void CMsmmPolicyNotificationManager::RunL()
    {
    LOG_FUNC
    iErrorQueue.Remove(0);
    if (iErrorQueue.Count() > 0)
        {
        SendNotification();
        }
    }

void CMsmmPolicyNotificationManager::DoCancel()
    {
    LOG_FUNC
    iErrorQueue.Reset();
    iNotifier.CancelNotifier(KMountPolicyNotifierUid);
    }

CMsmmPolicyNotificationManager::CMsmmPolicyNotificationManager():
CActive(EPriorityStandard)
    {
    LOG_FUNC
    CActiveScheduler::Add(this);
    }

void CMsmmPolicyNotificationManager::ConstructL()
    {
    LOG_FUNC
    User::LeaveIfError(iNotifier.Connect());
    }

void CMsmmPolicyNotificationManager::SendNotification()
    {
    LOG_FUNC
    iNotifier.StartNotifierAndGetResponse(
        iStatus, KMountPolicyNotifierUid, iErrorQueue[0], iResponse);
    SetActive();
    }

// End of file
