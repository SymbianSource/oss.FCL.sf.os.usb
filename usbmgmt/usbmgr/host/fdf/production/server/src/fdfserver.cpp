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

/**
 @file
 @internalComponent
*/

#include "fdfserver.h"
#include "fdfsession.h"
#include <usb/usblogger.h>
#include "utils.h"
#include "fdfapi.h"
#include "fdf.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef _DEBUG
PANICCATEGORY("fdfsrv");
#endif

void CFdfServer::NewLC()
	{
	LOG_STATIC_FUNC_ENTRY

	CFdfServer* self = new(ELeave) CFdfServer;
	CleanupStack::PushL(self);
	// StartL is where the kernel checks that there isn't already an instance
	// of the same server running, so do it before ConstructL.
	self->StartL(KUsbFdfServerName);
	self->ConstructL();
	}

CFdfServer::~CFdfServer()
	{
	LOG_FUNC

	delete iFdf;
	}

CFdfServer::CFdfServer()
 :	CServer2(CActive::EPriorityHigh)
	{
	}

void CFdfServer::ConstructL()
	{
	LOG_FUNC

	iFdf = CFdf::NewL();
	}

CSession2* CFdfServer::NewSessionL(const TVersion& aVersion,
	const RMessage2& aMessage) const
	{
	LOG_LINE
	LOG_FUNC;
	LOGTEXT4(_L8("\taVersion = (%d,%d,%d)"), aVersion.iMajor, aVersion.iMinor, aVersion.iBuild);
	(void)aMessage;

	// Check if we already have a session open.
	if ( iSession )
		{
		LEAVEL(KErrInUse);
		}

	// In the production system, check the secure ID of the prospective
	// client. It should be that of USBSVR.
	// For unit testing, don't check the SID of the connecting client (it will
	// not be USBSVR but the FDF Unit Test server).
#ifndef __OVER_DUMMYUSBDI__

#ifndef __TEST_FDF__
	// NB When using t_fdf, this SID check needs disabling- t_fdf has yet
	// another SID.
	_LIT_SECURE_ID(KUsbsvrSecureId, 0x101fe1db);
	// Contrary to what the sysdoc says on SecureId, we specifically *don't*
	// use a _LIT_SECURITY_POLICY_S0 here. This is because (a) we emit our own
	// diagnostic messages, and (b) we don't want configuring this security
	// check OFF to allow any client to pass and thereby break our
	// architecture.
	TInt error = ( aMessage.SecureId() == KUsbsvrSecureId ) ? KErrNone : KErrPermissionDenied;
	LEAVEIFERRORL(error);
#endif // __TEST_FDF__

#endif // __OVER_DUMMYUSBDI__

	// Version number check...
	TVersion v(KUsbFdfSrvMajorVersionNumber,
		KUsbFdfSrvMinorVersionNumber,
		KUsbFdfSrvBuildNumber);

	if ( !User::QueryVersionSupported(v, aVersion) )
		{
		LEAVEL(KErrNotSupported);
		}

	CFdfServer* ncThis = const_cast<CFdfServer*>(this);
	ncThis->iSession = new(ELeave) CFdfSession(*iFdf, *ncThis);
	ASSERT_DEBUG(ncThis->iFdf);
	ncThis->iFdf->SetSession(iSession);

	LOGTEXT2(_L8("\tiSession = 0x%08x"), iSession);
	return iSession;
	}

void CFdfServer::SessionClosed()
	{
	LOG_FUNC

	ASSERT_DEBUG(iSession);
	iSession = NULL;
	iFdf->SetSession(NULL);

	LOGTEXT(_L8("\tno remaining sessions- shutting down"));
	// This returns control to the server boilerplate in main.cpp. This
	// destroys all the objects, which includes signalling device detachment
	// to any extant FDCs.
	// The session object could perfectly well do this in its destructor but
	// it's arguably more clear for the server to do it as it's the server
	// that's created immediately before calling CActiveScheduler::Start in
	// main.cpp.
	CActiveScheduler::Stop();
	}
