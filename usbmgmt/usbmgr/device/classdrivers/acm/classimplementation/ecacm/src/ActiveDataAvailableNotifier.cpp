/*
* Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <e32std.h>
#include <d32usbc.h>
#include "ActiveDataAvailableNotifier.h"
#include "AcmConstants.h"
#include "AcmPanic.h"
#include "AcmUtils.h"
#include "NotifyDataAvailableObserver.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CActiveDataAvailableNotifier::CActiveDataAvailableNotifier(
								MNotifyDataAvailableObserver& aParent, 
								RDevUsbcClient& aLdd,
								TEndpointNumber aEndpoint)
 :	CActive(KEcacmAOPriority), 
	iParent(aParent),
	iLdd(aLdd),
	iEndpoint(aEndpoint)
/**
 * Constructor.
 *
 * @param aParent The object that will be notified if a 
 * NotifyDataAvailable() request has been made and incoming data 
 * arrives at the LDD.
 * @param aLdd The LDD handle to be used for posting read requests.
 * @param aEndpoint The endpoint to read from.
 */
	{
	CActiveScheduler::Add(this);
	}

CActiveDataAvailableNotifier::~CActiveDataAvailableNotifier()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	Cancel();
	}

CActiveDataAvailableNotifier* CActiveDataAvailableNotifier::NewL(
								MNotifyDataAvailableObserver& aParent, 
								RDevUsbcClient& aLdd,
								TEndpointNumber aEndpoint)
/**
 * Standard two phase constructor.
 *
 * @param aParent The object that will be notified if a 
 * NotifyDataAvailable() request has been made and incoming data 
 * arrives at the LDD.
 * @param aLdd The LDD handle to be used for posting read requests.
 * @param aEndpoint The endpoint to read from.
 * @return Ownership of a new CActiveReadOneOrMoreReader object.
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CActiveDataAvailableNotifier* self = 
		new(ELeave) CActiveDataAvailableNotifier(aParent, aLdd, aEndpoint);
	return self;
	}

void CActiveDataAvailableNotifier::NotifyDataAvailable()
/**
 * When incoming data arrives at the LDD notify the caller.
 */
	{
	LOGTEXT(_L8(">>CActiveDataAvailableNotifier::NotifyDataAvailable"));

	iLdd.ReadOneOrMore(iStatus, iEndpoint, iUnusedBuf, 0);
	SetActive();

	LOGTEXT(_L8("<<CActiveDataAvailableNotifier::NotifyDataAvailable"));
	}

void CActiveDataAvailableNotifier::DoCancel()
/**
 * Cancel an outstanding request.
 */
	{
	LOG_FUNC

	iLdd.ReadCancel(iEndpoint);
	}

void CActiveDataAvailableNotifier::RunL()
/**
 * This function will be called when the zero byte ReadOneOrMore() call on the LDD
 * completes. This could have been caused by the receipt of a Zero Length Packet in
 * which case there is no data available to be read. In this situation 
 * NotifyDataAvailable() is called again, otherwise the parent is notified.
 * We also have to be careful about getting into an infinite loop if the cable has 
 * been detached.
 */
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("\tiStatus = %d"), iStatus.Int());
	
	TBool complete = EFalse;
	TInt completeErr = KErrNone;

	TInt recBufSize;
	TInt err = iLdd.QueryReceiveBuffer(iEndpoint, recBufSize);
	if ( err == KErrNone )
		{
		if ( recBufSize != 0 )
			{
			// There is data available.
			complete = ETrue;
			completeErr = KErrNone;
			}
		else
			{
			// There is no data available. This may be because we got a ZLP, but 
			// before we simply repost the notification, check to see if the LDD 
			// is still working. If there isn't then we should complete to the 
			// client to avoid getting ourselves into an infinite loop.
			if ( iStatus.Int() == KErrNone )
				{
				NotifyDataAvailable();
				}
			else
				{
				complete = ETrue;
				// The Active Reader and Active Writer objects pass LDD-specific 
				// errors straight up the stack, so I don't see a problem with 
				// doing the same here. [As opposed to genericising them into 
				// KErrCommsLineFail for instance.]
				completeErr = iStatus.Int();
				}
			}
		}
	else // QueryReceiveBuffer failed
		{
		complete = ETrue;
		completeErr = err;
		}

	if ( complete )
		{
		iParent.NotifyDataAvailableCompleted(completeErr);
		}
	}

//
// End of file
