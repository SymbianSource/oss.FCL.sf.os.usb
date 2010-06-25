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

#include "msmmserver.h"
#include "msmm_internal_def.h"
#include "msmmsession.h"
#include "msmmengine.h"
#include "eventqueue.h"
#include "msmmterminator.h"
#include "msmmdismountusbdrives.h"

#include <usb/hostms/msmmpolicypluginbase.h>
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmServer");
#endif

//  Static public functions
TInt CMsmmServer::ThreadFunction()
    {
    TInt ret = KErrNone;
    
    __UHEAP_MARK;
    // Create the cleanup stack
    CTrapCleanup* cleanupStack = CTrapCleanup::New();
    if (cleanupStack)
        {
#ifdef __FLOG_ACTIVE
        (void)CUsbLog::Connect();
#endif

        TRAP(ret, ThreadFunctionL());

#ifdef __FLOG_ACTIVE
        CUsbLog::Close();
#endif
        
        delete cleanupStack;
        }
    else
        {
        ret = KErrNoMemory;
        }
    __UHEAP_MARKEND;
    
    return ret;
    }

void CMsmmServer::ThreadFunctionL()
    {
    LOG_STATIC_FUNC_ENTRY
    
    TSecureId creatorSID = User::CreatorSecureId();
    if (KFDFWSecureId != creatorSID)
        {
        // Only FDF process can be the creator of the MSMM server
        User::Leave(KErrPermissionDenied);
        }
    
    // Create and install the active scheduler
    CActiveScheduler* scheduler = new(ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);

    // Create the server and leave it on cleanup stack
    CMsmmServer::NewLC();

    // Signal the client that the server is up and running
    RProcess::Rendezvous(KErrNone);
    
    RThread().SetPriority(EPriorityAbsoluteHigh);

    // Start the active scheduler, the server will now service 
    // request messages from clients.
    CActiveScheduler::Start();

    // Free the server and active scheduler.
    CleanupStack::PopAndDestroy(2, scheduler);
    }

CPolicyServer::TCustomResult CMsmmServer::CustomSecurityCheckL(
    const RMessage2&  aMsg,
     TInt&  /*aAction*/,
     TSecurityInfo&  /*aMissing*/)
 {
     CPolicyServer::TCustomResult returnValue = CPolicyServer::EFail;    
     
     TSecureId ClientSID = aMsg.SecureId();
 
     if (KFDFWSecureId == ClientSID)
         {
         returnValue = CPolicyServer::EPass;
         }     
     else if ((KSidHbDeviceDialogAppServer == ClientSID) && SessionNumber() > 0)
         {
         returnValue = CPolicyServer::EPass;
         }
     return returnValue;
 }

// Public functions
// Construction and destruction
CMsmmServer* CMsmmServer::NewLC()
    {
    LOG_STATIC_FUNC_ENTRY
    CMsmmServer* self = new (ELeave) CMsmmServer(EPriorityHigh);
    CleanupStack::PushL(self);
    
    // Create a server with unique server name
    self->StartL(KMsmmServerName);
    self->ConstructL();
    
    return self;
    }

CMsmmServer::~CMsmmServer()
    {
    LOG_FUNC
    delete iPolicyPlugin;
    delete iEventQueue;
    delete iEngine;
    delete iTerminator;
    delete iDismountErrData;
    delete iDismountManager;
    REComSession::FinalClose();

#ifndef __OVER_DUMMYCOMPONENT__
    iFs.RemoveProxyDrive(KPROXYDRIVENAME);
    iFs.Close();
#endif
    }
    
    // CMsmmServer APIs
CSession2* CMsmmServer::NewSessionL(const TVersion& aVersion, 
        const RMessage2& aMessage) const
    {
    LOG_FUNC
    
    if (KMaxClientCount <= SessionNumber())
        {
        // There is a connection to MSMM server already.
        // Currently design of MSMM can have two clients, one FDF and the other Indicator UI 
        // at any time.
        User::Leave(KErrInUse);
        }
    
    // Check the client-side API version number against the server version 
    // number.
    TVersion serverVersion(KMsmmServMajorVersionNumber,
        KMsmmServMinorVersionNumber, KMsmmServBuildVersionNumber);
    
    if (!User::QueryVersionSupported(serverVersion, aVersion))
        {
        // Server version incompatible with client-side API
        PanicClient(aMessage, ENoSupportedVersion);
        User::Leave(KErrNotSupported);
        }

    // Version number is OK - create the session
    return CMsmmSession::NewL(*(const_cast<CMsmmServer*>(this)), *iEventQueue);
    }

TInt CMsmmServer::SessionNumber() const
    {
    LOG_FUNC
    
    return iNumSessions;
    }

void CMsmmServer::AddSession()
    {
    LOG_FUNC
    
    ++iNumSessions;
    iTerminator->Cancel();
    }

void CMsmmServer::RemoveSession()
    {
    LOG_FUNC
    
    --iNumSessions;
    if (iNumSessions == 0)
        {
        // Discard all pending adding interface events from queue
        // and cancel event handler if a adding events is being 
        // handled in it.
        iEventQueue->Finalize();
        iTerminator->Cancel();
        iTerminator->Start();
        }
    }

void CMsmmServer::DismountUsbDrivesL(TUSBMSDeviceDescription& aDevice)
    {
    LOG_FUNC
    delete iDismountManager;
    iDismountManager = NULL;
    iDismountManager= CMsmmDismountUsbDrives::NewL();
    
    //Also notify the MSMM plugin of beginning of dismounting     
    iDismountErrData->iError = EHostMsEjectInProgress;
    iDismountErrData->iE32Error = KErrNone;
    iDismountErrData->iManufacturerString = aDevice.iManufacturerString;
    iDismountErrData->iProductString = aDevice.iProductString;
    iDismountErrData->iDriveName = 0x0;
   
    TRAP_IGNORE(iPolicyPlugin->SendErrorNotificationL(*iDismountErrData));

    // Start dismounting
    iDismountManager->DismountUsbDrives(*iPolicyPlugin, aDevice);
    }

//  Private functions 
// CMsmmServer Construction
CMsmmServer::CMsmmServer(TInt aPriority)
    :CPolicyServer(aPriority, KMsmmServerSecurityPolicy, EUnsharableSessions)
    {
    LOG_FUNC
    //
    }

void CMsmmServer::ConstructL()
    {
    LOG_FUNC
    
    iEngine = CMsmmEngine::NewL();
    iEventQueue = CDeviceEventQueue::NewL(*this);
    iTerminator = CMsmmTerminator::NewL(*iEventQueue);
    iPolicyPlugin = CMsmmPolicyPluginBase::NewL();
    iDismountErrData = new (ELeave) THostMsErrData;
    if (!iPolicyPlugin)
        {
        // Not any policy plugin implementation available
        PanicServer(ENoPolicyPlugin);
        }
    
    // Initalize RFs connection and add the ELOCAL file system to file server
    User::LeaveIfError(iFs.Connect());

#ifndef __OVER_DUMMYCOMPONENT__
    TInt ret(KErrNone);
    ret = iFs.AddProxyDrive(KFSEXTNAME);
    if ((KErrNone != ret) && (KErrAlreadyExists !=  ret))
        {
        User::Leave(ret);
        }
#endif
    
    // Start automatic shutdown timer
    iTerminator->Start();
    }

// End of file
