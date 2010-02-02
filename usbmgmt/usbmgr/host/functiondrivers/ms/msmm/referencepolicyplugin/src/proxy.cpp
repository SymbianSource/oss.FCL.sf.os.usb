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

// INCLUDE FILES
#include <e32std.h>
#include <ecom/implementationproxy.h>
#include <usb/usblogger.h>

#include "referencepolicyplugin.h"
#include "referenceplugin.hrh"
 
#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "UsbHostMsmmRefPP");
#endif

// Provides a key value pair table, this is used to identify
// the correct construction function for the requested interface.
const TImplementationProxy ImplementationTable[] =
    {
	IMPLEMENTATION_PROXY_ENTRY(KUidMsmmPolicyPluginImp,
	        CReferencePolicyPlugin::NewL)
	};

// Function used to return an instance of the proxy table.
EXPORT_C const TImplementationProxy* ImplementationGroupProxy(
        TInt& aTableCount)
    {
    LOGTEXT(_L(">>ImplementationGroupProxy()"));
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    LOGTEXT(_L("<<ImplementationGroupProxy()"));
    return ImplementationTable;
    }

// End of file
