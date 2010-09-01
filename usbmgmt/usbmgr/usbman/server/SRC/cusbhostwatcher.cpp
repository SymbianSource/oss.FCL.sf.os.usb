/*
* Copyright (c) 2008-2009 Nokia Corporation and/or its subsidiary(-ies).
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


#include "cusbhostwatcher.h"
#include <usb/usblogger.h>
#include "cusbhost.h"


#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "hoststatewatcher");
#endif

/*
 * 	Base class for USB Host watchers
 */
CActiveUsbHostWatcher::CActiveUsbHostWatcher(RUsbHostStack& aUsbHostStack, 
											 MUsbHostObserver& aOwner,
											 TUint aWatcherId):
	CActive(CActive::EPriorityStandard),
	iUsbHostStack(aUsbHostStack),
	iOwner(aOwner),
	iWatcherId(aWatcherId)
	{
	LOG_FUNC
	CActiveScheduler::Add(this);
	}

CActiveUsbHostWatcher::~CActiveUsbHostWatcher()
	{
	LOG_FUNC
	Cancel();
	}

void CActiveUsbHostWatcher::RunL()
	{
	LOG_FUNC

	ASSERT(iStatus.Int() == KErrNone);
	iOwner.NotifyHostEvent(iWatcherId);
	Post();
	}



/*
 * 	Monitors host events (attach/load/detach)
 */
CActiveUsbHostEventWatcher* CActiveUsbHostEventWatcher::NewL(
												RUsbHostStack& aUsbHostStack, 
												MUsbHostObserver& aOwner,
												TDeviceEventInformation& aHostEventInfo)
	{
	CActiveUsbHostEventWatcher* self = new (ELeave) CActiveUsbHostEventWatcher(aUsbHostStack,aOwner,aHostEventInfo);
	return self;
	}


CActiveUsbHostEventWatcher::CActiveUsbHostEventWatcher(
									RUsbHostStack& aUsbHostStack,
									MUsbHostObserver& aOwner,
									TDeviceEventInformation& aHostEventInfo)
									:CActiveUsbHostWatcher(aUsbHostStack,
														   aOwner,
														   KHostEventMonitor)
														 , iHostEventInfo(aHostEventInfo)

	{
	LOG_FUNC
	}

CActiveUsbHostEventWatcher::~CActiveUsbHostEventWatcher()
	{
	LOG_FUNC
	Cancel();
	}

void CActiveUsbHostEventWatcher::Post()
	{
	LOG_FUNC

	iUsbHostStack.NotifyDeviceEvent(iStatus, iHostEventInfo);
	SetActive();
	}

void CActiveUsbHostEventWatcher::DoCancel()
	{
	LOG_FUNC

	iUsbHostStack.NotifyDeviceEventCancel();
	}


/*
 * 	Monitors device monitor events
 */

CActiveUsbHostMessageWatcher* CActiveUsbHostMessageWatcher::NewL(
		RUsbHostStack& aUsbHostStack, 
		MUsbHostObserver& aOwner,
		TInt& aHostMessage)
	{
	CActiveUsbHostMessageWatcher* self = new (ELeave)CActiveUsbHostMessageWatcher(aUsbHostStack,aOwner,aHostMessage);
	return self;
	}

CActiveUsbHostMessageWatcher::~CActiveUsbHostMessageWatcher()
	{
	LOG_FUNC

	Cancel();
	}

CActiveUsbHostMessageWatcher::CActiveUsbHostMessageWatcher(
										RUsbHostStack& aUsbHostStack,
										MUsbHostObserver& aOwner,
										TInt& aHostMessage)
										:CActiveUsbHostWatcher(aUsbHostStack,
															   aOwner,
															   KHostMessageMonitor)
										, iHostMessage(aHostMessage)
	{
	LOG_FUNC
	}

void CActiveUsbHostMessageWatcher::Post()
	{
	LOG_FUNC
	
	iUsbHostStack.NotifyDevmonEvent(iStatus, iHostMessage);
	SetActive();
	}


void CActiveUsbHostMessageWatcher::DoCancel()
	{
	LOG_FUNC
	
	iUsbHostStack.NotifyDevmonEventCancel();
	}

