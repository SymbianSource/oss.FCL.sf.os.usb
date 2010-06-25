/*
* Copyright (c) 2007-2008 Nokia Corporation and/or its subsidiary(-ies).
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
* Description:  Active Object to dismount usb drives
*
*/


#include <usb/usblogger.h>
#include <usb/hostms/srverr.h>
#include <usb/hostms/msmmpolicypluginbase.h>

#include "msmmdismountusbdrives.h"

const TInt KDismountTimeOut   = 6000000; // 6 seconds


#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

CMsmmDismountUsbDrives::~CMsmmDismountUsbDrives()
    {
    LOG_FUNC
    Cancel(); 
    delete iDismountTimer;    
    iRFs.Close();    
    }

/**
 * Symbian two phase constructor
 */
CMsmmDismountUsbDrives* CMsmmDismountUsbDrives::NewL()
    {
    LOG_STATIC_FUNC_ENTRY    
    CMsmmDismountUsbDrives* self = CMsmmDismountUsbDrives::NewLC();
    CleanupStack::Pop(self);    
    return self;
    }

/**
 * Symbian two phase constructor. Object pushed to cleanup stack
 */
CMsmmDismountUsbDrives* CMsmmDismountUsbDrives::NewLC()
    {
    LOG_STATIC_FUNC_ENTRY    
    CMsmmDismountUsbDrives* self = new (ELeave) CMsmmDismountUsbDrives();
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

/**
 * Check the status of current dismount request and continue issuing next if no error
 */
void CMsmmDismountUsbDrives::RunL()
    {
    LOG_FUNC    
    
    iDismountTimer->CancelTimer();    
    
    // Indicates there has been an error dismounting a usb drive, report immediately to MSMM plugin and 
    // abort the process
    if ( iStatus != KErrNone )
        {
        CompleteDismountRequest( iStatus.Int() );
        }
    // Indicates we have reached the end of all usb drives dismounting, in other words a success condition
    else if ( iDriveIndex == KMaxDrives )
        {
        CompleteDismountRequest( KErrNone );
        }
    // We still have more drives to traverse
    else if ( iDriveIndex < KMaxDrives )
        {
        DoDismount();
        }
    }

/**
 * Cancel pending notifier and those in queue 
 */
void CMsmmDismountUsbDrives::DoCancel()
    {
    LOG_FUNC
    iRFs.NotifyDismountCancel(iStatus);
    }

CMsmmDismountUsbDrives::CMsmmDismountUsbDrives()
    : CActive(EPriorityStandard)
    {
    LOG_FUNC    
    CActiveScheduler::Add(this);    
    }

void CMsmmDismountUsbDrives::ConstructL()
    {
    LOG_FUNC    
    User::LeaveIfError( iRFs.Connect());
    iDismountTimer = CDismountTimer::NewL(this);
    }

/**
 * Dismount usb drives
 */
void CMsmmDismountUsbDrives::DismountUsbDrives(CMsmmPolicyPluginBase& aPlugin, TUSBMSDeviceDescription& aDevice)
    {    
    LOG_FUNC
    Cancel();
    iPlugin = &aPlugin;
    TUSBMSDeviceDescription& device = iDevicePkgInfo();
    device = aDevice;
    iDriveIndex = 0;
    iRFs.DriveList( iDriveList );
    DoDismount();
    }

/**
 * Callback to CMsmmPolicyPluginBase with either success or failure message
 */
void CMsmmDismountUsbDrives::CompleteDismountRequest(const TInt aResult)
    {
    THostMsErrData data;
    if( aResult == KErrNone )
        data.iError = EHostMsErrNone;
    else
        data.iError = EHostMsErrInUse;
    data.iE32Error = aResult;
    data.iManufacturerString = iDevicePkgInfo().iManufacturerString;
    data.iProductString = iDevicePkgInfo().iProductString;
    data.iDriveName = 0x0;
   
    TRAP_IGNORE(iPlugin->SendErrorNotificationL(data));
    }

/**
 * Dismount next usb drive
 */
void CMsmmDismountUsbDrives::DoDismount()
    {
    LOG_FUNC        
    TDriveInfo info;
    TInt err = KErrNone;
    for ( ; iDriveIndex < KMaxDrives; iDriveIndex++ )
        {
        if ( iDriveList[iDriveIndex] )
            {
            err = iRFs.Drive( info , iDriveIndex );            
            if ( info.iConnectionBusType == EConnectionBusUsb &&                 
                 info.iDriveAtt & KDriveAttExternal && 
                 err == KErrNone  )
                {
                LOGTEXT(_L("CMsmmDismountUsbDrives::DoDismount Dismount notify request "));    
                iRFs.NotifyDismount( iDriveIndex, iStatus, EFsDismountNotifyClients );                
                iDismountTimer->StartTimer();
                SetActive();
                return;
                }                     
            }
        }
    // Indicates we have gone through all the drives and no more usb drives left to request dismount
    CompleteDismountRequest( KErrNone );
    }


/**
 * Callback function from CDismountTimer after 6 seconds indicating a usb drive is not released by another process, report it as an error
 */
void CMsmmDismountUsbDrives::TimerExpired()
    {
    LOG_FUNC    
    
    Cancel();
    iDismountTimer->CancelTimer();    
    CompleteDismountRequest( KErrInUse );    
    }    

//CDismountTimer

CDismountTimer* CDismountTimer::NewL( MTimerNotifier* aTimeOutNotify)
    {
    LOG_STATIC_FUNC_ENTRY    
    CDismountTimer* self = CDismountTimer::NewLC( aTimeOutNotify );
    CleanupStack::Pop(self);
    return self;
    }

CDismountTimer* CDismountTimer::NewLC( MTimerNotifier* aTimeOutNotify )
    {
    LOG_STATIC_FUNC_ENTRY    
    CDismountTimer* self = new (ELeave) CDismountTimer( aTimeOutNotify );
    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
    }

CDismountTimer::CDismountTimer( MTimerNotifier* aTimeOutNotify):
    CTimer(EPriorityStandard), 
    iNotify(aTimeOutNotify)    
    {
    LOG_FUNC    
    }    

CDismountTimer::~CDismountTimer()
    {
    LOG_FUNC    
    Cancel();
    }

void CDismountTimer::ConstructL()
    {
    LOG_FUNC    
    if ( !iNotify )    
        {
        User::Leave(KErrArgument);    
        }
    CTimer::ConstructL();
    CActiveScheduler::Add(this);
    }

void CDismountTimer::RunL()
    {
    LOG_FUNC    
    // Timer request has completed, so notify the timer's owner
    iNotify->TimerExpired();
    }
void CDismountTimer::CancelTimer()
    {
    LOG_FUNC    
    Cancel();    
    }

void CDismountTimer::StartTimer()
    {
    LOG_FUNC
    After( KDismountTimeOut );  
    }
// End of File
