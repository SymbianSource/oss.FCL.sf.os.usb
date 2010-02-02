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
* Talks directly to the USB Logical Device Driver (LDD) and 
* watches any state changes
*
*/

/**
 @file
*/

#include <usb/usblogger.h>
#include "CUsbScheduler.h"
#include "CUsbDeviceStateWatcher.h"
#include "CUsbDevice.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR");
#endif

/**
 * The CUsbDeviceStateWatcher::NewL method
 *
 * Constructs a new CUsbDeviceStateWatcher object
 *
 * @internalComponent
 * @param	aOwner	The device that owns the state watcher
 * @param	aLdd	A reference to the USB Logical Device Driver
 *
 * @return	A new CUsbDeviceStateWatcher object
 */
CUsbDeviceStateWatcher* CUsbDeviceStateWatcher::NewL(CUsbDevice& aOwner, RDevUsbcClient& aLdd)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbDeviceStateWatcher* r = new (ELeave) CUsbDeviceStateWatcher(aOwner, aLdd);
	return r;
	}


/**
 * The CUsbDeviceStateWatcher::~CUsbDeviceStateWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbDeviceStateWatcher::~CUsbDeviceStateWatcher()
	{
	LOGTEXT2(_L8(">CUsbDeviceStateWatcher::~CUsbDeviceStateWatcher (0x%08x)"), (TUint32) this);
	Cancel();
	}


/**
 * The CUsbDeviceStateWatcher::CUsbDeviceStateWatcher method
 *
 * Constructor
 *
 * @param	aOwner	The device that owns the state watcher
 * @param	aLdd	A reference to the USB Logical Device Driver
 */
CUsbDeviceStateWatcher::CUsbDeviceStateWatcher(CUsbDevice& aOwner, RDevUsbcClient& aLdd)
	: CActive(CActive::EPriorityStandard), iOwner(aOwner), iLdd(aLdd)
	{
	CActiveScheduler::Add(this);
	}

/**
 * Called when the USB device changes its state.
 */
void CUsbDeviceStateWatcher::RunL()
	{
	if (iStatus.Int() != KErrNone)
		{
		LOGTEXT2(_L8("CUsbDeviceStateWatcher::RunL() - Error = %d"), iStatus.Int());
		return;
		}

	LOGTEXT2(_L8("CUsbDeviceStateWatcher::RunL() - State Changed to %d"), iState);

	if (!(iState & KUsbAlternateSetting))
		iOwner.SetDeviceState((TUsbcDeviceState) iState);

	LOGTEXT(_L8("CUsbDeviceStateWatcher::RunL() - About to call DeviceStatusNotify"));
	iLdd.AlternateDeviceStatusNotify(iStatus, iState);
	SetActive();
	LOGTEXT(_L8("CUsbDeviceStateWatcher::RunL() - Called DeviceStatusNotify"));
	}


/**
 * Automatically called when the state watcher is cancelled.
 */
void CUsbDeviceStateWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.AlternateDeviceStatusNotifyCancel();
	}


/**
 * Instructs the state watcher to start watching.
 */
void CUsbDeviceStateWatcher::Start()
	{
	LOG_FUNC
	iLdd.AlternateDeviceStatusNotify(iStatus, iState);
	SetActive();
	}
