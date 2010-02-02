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

#include "activewaitforbusevent.h"
#include <usb/usblogger.h>
#include "utils.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif


CActiveWaitForBusEvent::CActiveWaitForBusEvent(RUsbHubDriver& aHubDriver,
											   RUsbHubDriver::TBusEvent& aBusEvent,
											   MBusEventObserver& aObserver)
:	CActive(CActive::EPriorityStandard),
	iHubDriver(aHubDriver),
	iBusEvent(aBusEvent),
	iObserver(aObserver)
	{
	LOG_FUNC

	CActiveScheduler::Add(this);
	}

CActiveWaitForBusEvent::~CActiveWaitForBusEvent()
	{
	LOG_FUNC

	Cancel();
	}

CActiveWaitForBusEvent* CActiveWaitForBusEvent::NewL(RUsbHubDriver& aHubDriver,
													 RUsbHubDriver::TBusEvent& aBusEvent,
													 MBusEventObserver& aObserver)
	{
	CActiveWaitForBusEvent* self = new(ELeave) CActiveWaitForBusEvent(aHubDriver, aBusEvent, aObserver);
	return self;
	}

void CActiveWaitForBusEvent::Wait()
	{
	LOG_FUNC

	iHubDriver.WaitForBusEvent(iBusEvent, iStatus);
	SetActive();
	}

void CActiveWaitForBusEvent::RunL()
	{
	LOG_LINE
	LOG_FUNC
	LOGTEXT3(_L8("\tiStatus = %d , iBusEvent.iError=%d "), iStatus.Int(),iBusEvent.iError);

	iObserver.MbeoBusEvent();
	}

void CActiveWaitForBusEvent::DoCancel()
	{
	LOG_FUNC

	iHubDriver.CancelWaitForBusEvent();
	}
