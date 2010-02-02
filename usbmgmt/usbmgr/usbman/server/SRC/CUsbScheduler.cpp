/*
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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
*/

#include <usb/usblogger.h>
#include "CUsbScheduler.h"
#include "CUsbServer.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif

/**
 * The CUsbScheduler::NewL method
 *
 * Creates a new Active scheduler
 *
 * @internalComponent
 */
CUsbScheduler* CUsbScheduler::NewL()
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbScheduler* self = new(ELeave) CUsbScheduler;
	return self;
	}

/**
 * The CUsbScheduler::~CUsbScheduler method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbScheduler::~CUsbScheduler()
	{
	// Note that though we store a pointer to the server,
	// we do not own it (it is owned by the cleanup stack)
	// and our pointer is only used to error the server
	// if we have been given a reference to it.
	}

/**
 * The CUsbScheduler::SetServer method
 *
 * Give us a reference to the server
 *
 * @internalComponent
 * @param	aServer	A reference to the server
 */
void CUsbScheduler::SetServer(CUsbServer& aServer)
	{
	iServer = &aServer;	
	}

/**
 * The CUsbScheduler::Error method
 *
 * Inform the server that an error has occurred
 *
 * @internalComponent
 * @param	aError	Error that has occurred
 */
void CUsbScheduler::Error(TInt aError) const
	{
	LOGTEXT2(_L8("CUsbScheduler::Error aError=%d"), aError);

	if (iServer)
		{
		iServer->Error(aError);
		}
	}

