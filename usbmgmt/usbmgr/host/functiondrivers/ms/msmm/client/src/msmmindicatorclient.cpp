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

#include <usb/usblogger.h>
#include <e32cmn.h>

#include "srvdef.h"
#include "msmmindicatorclient.h"
 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmIndicatorClient");
#endif

// Costants
const TInt KConnectRetry = 0x2;


//---------------------------------------------------------------------------
// RHostMassStorage
//
// Public member functions

EXPORT_C TInt RHostMassStorage::Connect()
    {
    LOG_FUNC
    
    TInt retry = KConnectRetry; // Attempt to add a session to MSMM Server twice
    TInt ret(KErrNone);
    FOREVER
        {
        ret = CreateSession(KMsmmServerName, Version(), KDefaultMessageSlots);
        // We are not allowed to start the server
        if ((KErrNotFound == ret) || (KErrServerTerminated == ret))
            {
            LOGTEXT2(_L("Underlying error value = %d"), ret)
            return KErrNotReady;
            }
        if ( KErrNone == ret )
            {
            break;
            }
        if ((--retry) == 0)
            {
            break;
            }
        }    
    return ret; 
    }

EXPORT_C TInt RHostMassStorage::Disconnect()
    {
    LOG_FUNC
    
    Close();
    return KErrNone;
    }

/**
 *  Called to validate the version of the server we require for this API
 *  @return TVersion    The version of MSMM Server that supports this API
 */
EXPORT_C TVersion RHostMassStorage::Version() const
    {
    LOG_FUNC
    
    return TVersion(KMsmmServMajorVersionNumber,
                    KMsmmServMinorVersionNumber,
                    KMsmmServBuildVersionNumber);
    }

/** 
 * Dismount USB drives from File System asynchronously. The function will return immediately to the UI with complete status
 * The result of dismounting USB drives will be notified to the MSMM Plugin via CMsmmPolicyPluginBase
 * @return Error code of IPC.
*/

EXPORT_C TInt RHostMassStorage::EjectUsbDrives()
    {
    LOG_FUNC
    
    TInt ret(KErrNone);

    TIpcArgs usbmsIpcArgs;

    ret = Send(EHostMsmmServerEjectUsbDrives, usbmsIpcArgs);
    
    return ret;
    }


// End of file
