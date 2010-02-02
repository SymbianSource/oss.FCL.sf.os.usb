/*
* Copyright (c) 2006-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <e32base.h>
#include <d32usbc.h>
#include <usb/usblogger.h>
#include "AcmPanic.h"
#include "linkstatenotifier.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ECACM");
#endif

CLinkStateNotifier* CLinkStateNotifier::NewL(MLinkStateObserver& aParent, RDevUsbcClient& aUsb)
	{
	LOG_STATIC_FUNC_ENTRY
	
	CLinkStateNotifier* self = new (ELeave) CLinkStateNotifier(aParent, aUsb);
	return self;
	}


CLinkStateNotifier::CLinkStateNotifier(MLinkStateObserver& aParent, RDevUsbcClient& aUsb)
	 : CActive(EPriorityStandard),
	   iParent(aParent), iUsb(aUsb)
	{
	LOG_FUNC
	
	CActiveScheduler::Add(this);
	}



/**
CObexUsbHandler destructor.
*/
CLinkStateNotifier::~CLinkStateNotifier()
	{
	LOG_FUNC
	Cancel();
	}


/**
Standard active object error function.

@return	KErrNone because currently nothing should cause this to be called.
*/
TInt CLinkStateNotifier::RunError(TInt /*aError*/)
	{
	return KErrNone;
	}


/**
This function will be called upon a change in the state of the device
(as set up in AcceptL).
*/
void CLinkStateNotifier::RunL()
	{
	LOGTEXT2(_L8("CObexUsbHandler::RunL called state=0x%X"), iUsbState);
	
	if (iStatus != KErrNone)
		{
		LOGTEXT2(_L8("CObexUsbHandler::RunL() - Error = %d"),iStatus.Int());
		LinkDown();
		iParent.MLSOStateChange(KDefaultMaxPacketTypeBulk);

		return;
		}

	if (!(iUsbState & KUsbAlternateSetting))
		{
		TUsbcDeviceState deviceState = static_cast<TUsbcDeviceState>(iUsbState);

		switch(deviceState)
			{
		case EUsbcDeviceStateUndefined:
			LinkDown();
			break;
				
		case EUsbcDeviceStateAttached:
		case EUsbcDeviceStatePowered:
		case EUsbcDeviceStateDefault:
		case EUsbcDeviceStateAddress:
		case EUsbcDeviceStateSuspended:
			break;

		case EUsbcDeviceStateConfigured:
			LinkUp();
			break;

		default:
			__ASSERT_DEBUG(false, _USB_PANIC(KAcmPanicCat, EPanicUnknownDeviceState));
			break;
			}
		}

	iParent.MLSOStateChange(iPacketSize || KDefaultMaxPacketTypeBulk);

	// Await further notification of a state change.
	iUsb.AlternateDeviceStatusNotify(iStatus, iUsbState);
	SetActive();
	}


/**
Standard active object cancellation function.
*/
void CLinkStateNotifier::DoCancel()
	{
	LOG_FUNC

	iUsb.AlternateDeviceStatusNotifyCancel();
	}



/**
Accept an incoming connection.
*/
void CLinkStateNotifier::Start()
	{
	LOG_FUNC
	iUsb.AlternateDeviceStatusNotify(iStatus, iUsbState);
	SetActive();
	}


void CLinkStateNotifier::LinkUp()
	{
	if (iUsb.CurrentlyUsingHighSpeed())
		{
		iPacketSize = KMaxPacketTypeBulkHS;
		}
	else
		{
		iPacketSize = KMaxPacketTypeBulkFS;
		}
	}


void CLinkStateNotifier::LinkDown()
	{
	iPacketSize = 0;
	}
