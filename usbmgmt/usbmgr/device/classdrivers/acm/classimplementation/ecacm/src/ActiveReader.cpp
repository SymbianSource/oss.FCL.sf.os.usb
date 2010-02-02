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
#include "ActiveReader.h"
#include "AcmConstants.h"
#include "AcmPanic.h"
#include "ReadObserver.h"
#include "AcmUtils.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CActiveReader::CActiveReader(MReadObserver& aParent, RDevUsbcClient& aLdd, TEndpointNumber aEndpoint)
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

CActiveReader::~CActiveReader()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	Cancel();
	}

CActiveReader* CActiveReader::NewL(MReadObserver& aParent, 
								   RDevUsbcClient& aLdd,
								   TEndpointNumber aEndpoint)
/**
 * Standard two phase constructor.
 *
 * @param aParent The object that will be notified when read requests 
 * complete.
 * @param aLdd The LDD handle to be used for posting read requests.
 * @param aEndpoint The endpoint to read from.
 * @return Ownership of a new CActiveReader object.
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CActiveReader* self = new(ELeave) CActiveReader(aParent, aLdd, aEndpoint);
	return self;
	}

void CActiveReader::Read(TDes8& aDes, TInt aLen)
/**
 * Read the given length of data from the LDD.
 *
 * @param aDes A descriptor into which to read.
 * @param aLen The length to read.
 */
	{
	LOG_FUNC

	iLdd.Read(iStatus, iEndpoint, aDes, aLen); 
	SetActive();
	}

void CActiveReader::DoCancel()
/**
 * Cancel an outstanding read.
 */
	{
	LOG_FUNC

	iLdd.ReadCancel(iEndpoint);
	}

void CActiveReader::RunL()
/**
 * This function will be called when the read completes. It notifies the 
 * parent class of the completion.
 */
	{
	LOG_LINE
	LOGTEXT2(_L8(">>CActiveReader::RunL iStatus=%d"), iStatus.Int());

	iParent.ReadCompleted(iStatus.Int());

	LOGTEXT(_L8("<<CActiveReader::RunL"));
	}

//
// End of file
