/*
* Copyright (c) 2005-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <e32base.h>
#include "acmserverimpl.h"
#include <usb/usblogger.h>
#include <acminterface.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ACMSVRCLI");
#endif

/** Constructor */
CAcmServerImpl::CAcmServerImpl() 
	{
	LOG_FUNC
	}
	   
/** Destructor */
CAcmServerImpl::~CAcmServerImpl()
	{
	LOG_FUNC

	iCommServ.Close();
	iAcmServerClient.Close();
	}

/**
2-phase construction.
@return Ownership of a new CAcmServerImpl.
*/
CAcmServerImpl* CAcmServerImpl::NewL()
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmServerImpl* self = new(ELeave) CAcmServerImpl;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CAcmServerImpl::ConstructL()
	{
	LOG_FUNC

	// In order to connect a session, the ECACM CSY must be loaded (it 
	// contains the server).
	LEAVEIFERRORL(iCommServ.Connect());
	LEAVEIFERRORL(iCommServ.LoadCommModule(KAcmCsyName));
	// NB RCommServ::Close undoes LoadCommModule.
	LEAVEIFERRORL(iAcmServerClient.Connect());
	// iCommServ is eventually cleaned up in our destructor. It must be held 
	// open at least as long as our session on the ACM server, otherwise 
	// there's a risk the ACM server will be pulled from under our feet.
	}

TInt CAcmServerImpl::CreateFunctions(const TUint aNoAcms, const TUint8 aProtocolNum, const TDesC& aAcmControlIfcName, const TDesC& aAcmDataIfcName)
	{
	LOG_FUNC

	return iAcmServerClient.CreateFunctions(aNoAcms, aProtocolNum, aAcmControlIfcName, aAcmDataIfcName);
	}

TInt CAcmServerImpl::DestroyFunctions(const TUint aNoAcms)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taNoAcms = %d"), aNoAcms);

	return iAcmServerClient.DestroyFunctions(aNoAcms);
	}
