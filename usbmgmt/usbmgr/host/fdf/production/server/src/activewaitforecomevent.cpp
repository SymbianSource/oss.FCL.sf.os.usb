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


#include "activewaitforecomevent.h"
#include <usb/usblogger.h>
#include "utils.h"


#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef _DEBUG
_LIT( KFdfEcomEventAOPanicCategory, "FdfEcomEventAO" );
#endif

CActiveWaitForEComEvent::CActiveWaitForEComEvent(MEComEventObserver& aObserver)
:	CActive(CActive::EPriorityStandard),
	iObserver(aObserver)
	{
	LOG_FUNC

	CActiveScheduler::Add(this);
	}

CActiveWaitForEComEvent::~CActiveWaitForEComEvent()
	{
	LOG_FUNC
	Cancel();
	iEComSession.Close();
	REComSession::FinalClose();
	}

CActiveWaitForEComEvent* CActiveWaitForEComEvent::NewL(MEComEventObserver& aObserver)
	{
	CActiveWaitForEComEvent* self = new(ELeave) CActiveWaitForEComEvent(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


void CActiveWaitForEComEvent::ConstructL()
	{
	iEComSession = REComSession::OpenL();
	}

void CActiveWaitForEComEvent::Wait()
	{
	LOG_FUNC
	iEComSession.NotifyOnChange(iStatus);
	SetActive();
	}

void CActiveWaitForEComEvent::RunL()
	{
	LOG_LINE
	LOG_FUNC
	iObserver.EComEventReceived();
	Wait();
	}

void CActiveWaitForEComEvent::DoCancel()
	{
	LOG_FUNC
	iEComSession.CancelNotifyOnChange(iStatus);
	}

TInt CActiveWaitForEComEvent::RunError(TInt aError)
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT2(_L8("ECOM change notification error = %d "), aError);
	__ASSERT_DEBUG(EFalse, _USB_PANIC(KFdfEcomEventAOPanicCategory, aError));
	return KErrNone;
	}
