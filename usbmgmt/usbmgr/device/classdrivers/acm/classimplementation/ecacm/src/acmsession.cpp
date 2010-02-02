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
* ACM session.
*
*/

/**
 @file
 @internalComponent
*/

#include "acmserverconsts.h"
#include "acmsession.h"
#include "AcmPortFactory.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CAcmSession* CAcmSession::NewL(MAcmController& aAcmController)
	{
	LOG_STATIC_FUNC_ENTRY

	CAcmSession* self = new(ELeave) CAcmSession(aAcmController);
	return self;
	}

CAcmSession::CAcmSession(MAcmController& aAcmController)
 :	iAcmController(aAcmController)
	{
	LOG_FUNC
	}

CAcmSession::~CAcmSession()
	{
	LOG_FUNC
	}
	
void CAcmSession::CreateFunctionsL(const RMessage2& aMessage)
	{
	LOG_FUNC

	RBuf acmControlIfcName, acmDataIfcName;

	TInt size = aMessage.GetDesLengthL(2);
	acmControlIfcName.CreateL(size);
	acmControlIfcName.CleanupClosePushL();
	aMessage.ReadL(2, acmControlIfcName);

	size = aMessage.GetDesLengthL(3);
	acmDataIfcName.CreateL(size);
	acmDataIfcName.CleanupClosePushL();
	aMessage.ReadL(3, acmDataIfcName);

	LOGTEXT5(_L("\taNoAcms = %d, aProtocolNum = %d, Control Ifc Name = %S, Data Ifc Name = %S"),
			aMessage.Int0(), aMessage.Int1(), &acmControlIfcName, &acmDataIfcName);

	LEAVEIFERRORL(iAcmController.CreateFunctions(aMessage.Int0(), aMessage.Int1(), acmControlIfcName, acmDataIfcName));

	CleanupStack::PopAndDestroy(2);
	}

void CAcmSession::ServiceL(const RMessage2& aMessage)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taMessage.Function() = %d"), aMessage.Function());

	switch ( aMessage.Function() )
		{
	case EAcmCreateAcmFunctions:
		{
		TRAPD (err, CreateFunctionsL(aMessage));
		aMessage.Complete(err);
		break;
		}

	case EAcmDestroyAcmFunctions:
		{
		iAcmController.DestroyFunctions(aMessage.Int0());
		aMessage.Complete(KErrNone);
		break;
		}
		
	default:
		// Unknown function, panic the user
		aMessage.Panic(KAcmSrvPanic, EAcmBadAcmMessage);
		break;
		}
	}
