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

#include <e32std.h>
#include <d32usbc.h>
#include "ActiveReadOneOrMoreReader.h"
#include "AcmConstants.h"
#include "AcmPanic.h"
#include "AcmUtils.h"
#include "ReadOneOrMoreObserver.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CActiveReadOneOrMoreReader::CActiveReadOneOrMoreReader(
								MReadOneOrMoreObserver& aParent, 
								RDevUsbcClient& aLdd,
								TEndpointNumber aEndpoint)
 :	CActive(KEcacmAOPriority), 
	iParent(aParent),
	iLdd(aLdd),
	iEndpoint(aEndpoint)
/**
 * Constructor.
 *
 * @param aParent The object that will be notified when read requests 
 * complete.
 * @param aLdd The LDD handle to be used for posting read requests.
 * @param aEndpoint The endpoint to read from.
 */
	{
	CActiveScheduler::Add(this);
	}

CActiveReadOneOrMoreReader::~CActiveReadOneOrMoreReader()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	Cancel();
	}

CActiveReadOneOrMoreReader* CActiveReadOneOrMoreReader::NewL(
								MReadOneOrMoreObserver& aParent, 
								RDevUsbcClient& aLdd,
								TEndpointNumber aEndpoint)
/**
 * Standard two phase constructor.
 *
 * @param aParent The object that will be notified when read requests 
 * complete.
 * @param aLdd The LDD handle to be used for posting read requests.
 * @param aEndpoint The endpoint to read from.
 * @return Ownership of a new CActiveReadOneOrMoreReader object.
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CActiveReadOneOrMoreReader* self = 
		new(ELeave) CActiveReadOneOrMoreReader(aParent, aLdd, aEndpoint);
	return self;
	}

void CActiveReadOneOrMoreReader::ReadOneOrMore(TDes8& aDes, TInt aLength)
/**
 * Read as much data as the LDD can supply up to the given limit.
 *
 * @param aDes A descriptor into which the data will be read.
 * @param aLength The length to read.
 */
	{
	LOGTEXT2(_L8(">>CActiveReadOneOrMoreReader::ReadOneOrMore "
		"aLength=%d"), aLength);

	iLdd.ReadOneOrMore(iStatus, iEndpoint, aDes, aLength);
	SetActive();

	LOGTEXT(_L8("<<CActiveReadOneOrMoreReader::ReadOneOrMore"));
	}

void CActiveReadOneOrMoreReader::DoCancel()
/**
 * Cancel an outstanding request.
 */
	{
	LOG_FUNC

	iLdd.ReadCancel(iEndpoint);
	}

void CActiveReadOneOrMoreReader::RunL()
/**
 * This function will be called when the request completes. It notifies the 
 * parent class of the completion.
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CActiveReadOneOrMoreReader::RunL iStatus=%d"), 
		iStatus.Int());

	iParent.ReadOneOrMoreCompleted(iStatus.Int());

	LOGTEXT(_L8("<<CActiveReadOneOrMoreReader::RunL"));
	}

//
// End of file
