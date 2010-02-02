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

#include "msmmclient.h"
#include <usb/usblogger.h>

#include <e32cmn.h>

#include "srvdef.h"
 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmClient");
#endif

// Costants
const TInt KConnectRetry = 0x2;

//---------------------------------------------------------------------------
//
// NON-MEMBER FUNCTIONS

static TInt StartServer()
    {
    LOG_STATIC_FUNC_ENTRY
    
    TInt ret = KErrNone;

    // Create the server process
    const TUidType serverUid(KNullUid, KNullUid, KMsmmServUid);
    RProcess server;
    _LIT(KNullCommand,"");
    ret = server.Create(KMsmmServerBinaryName, KNullCommand, serverUid);

    // Was server process created OK?
    if (KErrNone != ret)
        {
        return ret;
        }

    // Set up Rendezvous so that server thread can signal correct startup
    TRequestStatus serverDiedRequestStatus;
    server.Rendezvous(serverDiedRequestStatus);

    // Status flag should still be pending as we haven't 
    // resumed the server yet!
    if (serverDiedRequestStatus != KRequestPending)
        {
        server.Kill(0); // abort the startup here
        }
    else
        {
        server.Resume(); // start server
        }

    User::WaitForRequest(serverDiedRequestStatus);

    // determine the reason for the server exit
    TInt exitReason = (EExitPanic == server.ExitType()) ? 
            KErrGeneral : serverDiedRequestStatus.Int();
    server.Close();
    return exitReason;
    }

//---------------------------------------------------------------------------
// RMsmmSession
//
// Public member functions

EXPORT_C TInt RMsmmSession::Connect()
    {
    LOG_FUNC
    
    TInt retry = KConnectRetry; // Attempt connect twice then give up
    TInt ret(KErrNone);
    FOREVER
        {
        ret = CreateSession(KMsmmServerName, Version(), KDefaultMessageSlots);
        if ((KErrNotFound != ret) && (KErrServerTerminated != ret))
            {
            break;
            }

        if ((--retry) == 0)
            {
            break;
            }

        ret = StartServer();
        if ((KErrNone != ret) && (KErrAlreadyExists != ret))
            {
            break;
            }
        }

    if (KErrNone != ret)
        {
        LOGTEXT2(_L("Underlying error value = %d"), ret)
        ret = KErrCouldNotConnect;
        }
    
    return ret; 
    }

EXPORT_C TInt RMsmmSession::Disconnect()
    {
    LOG_FUNC
    
    Close();
    return KErrNone;
    }

// Called to provide the version number of the server we require for this API
EXPORT_C TVersion RMsmmSession::Version() const
    {
    LOG_FUNC
    
    return TVersion(KMsmmServMajorVersionNumber,
                    KMsmmServMinorVersionNumber,
                    KMsmmServBuildVersionNumber);
    }

EXPORT_C TInt RMsmmSession::AddFunction(
        const TUSBMSDeviceDescription& aDevice,
        TUint8 aInterfaceNumber, TUint32 aInterfaceToken)
    {
    LOG_FUNC
    
    TInt ret(KErrNone);
    
    TIpcArgs usbmsIpcArgs;
    iDevicePkg = aDevice; // Package the device description
    usbmsIpcArgs.Set(0, &iDevicePkg);
    usbmsIpcArgs.Set(1, aInterfaceNumber);
    usbmsIpcArgs.Set(2, aInterfaceToken);

    ret = SendReceive(EHostMsmmServerAddFunction, usbmsIpcArgs);
    
    return ret;
    }

EXPORT_C TInt RMsmmSession::RemoveDevice(TUint aDevice)
    {
    LOG_FUNC
    
    TInt ret(KErrNone);

    TIpcArgs usbmsIpcArgs(aDevice);

    ret = SendReceive(EHostMsmmServerRemoveDevice, usbmsIpcArgs);
    
    return ret;
    }

EXPORT_C TInt RMsmmSession::__DbgFailNext(TInt aCount)
    {
    LOG_FUNC
    
#ifdef _DEBUG
    return SendReceive(EHostMsmmServerDbgFailNext, TIpcArgs(aCount));
#else
    (void)aCount;
    return KErrNone;
#endif
    }

EXPORT_C TInt RMsmmSession::__DbgAlloc()
    {
    LOG_FUNC
    
#ifdef _DEBUG
    return SendReceive(EHostMsmmServerDbgAlloc);
#else
    return KErrNone;
#endif
    }

// End of file
