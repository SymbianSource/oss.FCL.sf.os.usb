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
* ACM server.
*
*/

/**
 @file
 @internalComponent
*/

#include <usb/usblogger.h>
#include "acmserver.h"
#include "acmsession.h"
#include "acmserversecuritypolicy.h"
#include "acmserverconsts.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CAcmServer* CAcmServer::NewL(MAcmController& aAcmController)
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmServer* self = new(ELeave) CAcmServer(aAcmController);
	CleanupStack::PushL(self);
	TInt err = self->Start(KAcmServerName);
	// KErrAlreadyExists is a success case (c.f. transient server boilerplate
	// code).
	if ( err != KErrAlreadyExists )
		{
		LEAVEIFERRORL(err);
		}
	CleanupStack::Pop(self);
	return self;
	}

CAcmServer::~CAcmServer()
	{
	LOG_FUNC
	}

CAcmServer::CAcmServer(MAcmController& aAcmController)
 :	CPolicyServer(CActive::EPriorityStandard, KAcmServerPolicy),
 	iAcmController(aAcmController)
 	{
	}

CSession2* CAcmServer::NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const
	{
	LOG_FUNC

	//Validate session as coming from UsbSvr
	static _LIT_SECURITY_POLICY_S0(KSidPolicy, 0x101fe1db);
	TBool auth = KSidPolicy.CheckPolicy(aMessage);
	if(!auth)
		{
		LEAVEIFERRORL(KErrPermissionDenied);
		}

	// Version number check...
	TVersion v(	KAcmSrvMajorVersionNumber,
				KAcmSrvMinorVersionNumber,
				KAcmSrvBuildNumber);

	if ( !User::QueryVersionSupported(v, aVersion) )
		{
		LEAVEIFERRORL(KErrNotSupported);
		}

	CAcmSession* sess = CAcmSession::NewL(iAcmController);
	LOGTEXT2(_L8("\tsess = 0x%08x"), sess);
	return sess;
	}
