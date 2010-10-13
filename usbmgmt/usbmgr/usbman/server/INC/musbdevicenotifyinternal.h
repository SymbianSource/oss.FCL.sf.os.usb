/*
* Copyright (c) 1997-2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef MUSBDEVICENOTIFYINTERNAL_H
#define MUSBDEVICENOTIFYINTERNAL_H

#include <MUsbDeviceNotify.h>
#include <musbthermalnotify.h>
/**
 * The MUsbDeviceNotifyInternal class
 * This is an internal class which is used for usb package only.
 */
class MUsbDeviceNotifyInternal : public MUsbDeviceNotify, public MUsbThermalNotify
	{
public:
	/**
	 * Called when the USB service state has changed
	 *
	 * @param aLastError The last error code detected
	 * @param aOldState The previous service state
	 * @param aNewState The new service state
	 */
	virtual void UsbServiceStateChange(TInt aLastError, TUsbServiceState aOldState, TUsbServiceState aNewState) = 0;

	/**
	 * Called when the USB device state has changed
	 *
	 * @param aLastError The last error code detected
	 * @param aOldState The previous device state
	 * @param aNewState The new device state
	 */
	virtual void UsbDeviceStateChange(TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState) = 0;

    /**
     * Called when the thermal infomation had changed
     *
     * @param aLastError The last error code detected
     * @param aNewValue The new value of thermal
     */	
	virtual void UsbThermalStateChange(TInt aLastError, TInt aNewValue) = 0;
	};

#endif	// MUSBDEVICENOTIFYINTERNAL_H
	