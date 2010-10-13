/*
* Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef MUSBTHERMALNOTIFY_H
#define MUSBTHERMALNOTIFY_H

#include <e32def.h>

// Class used to be used as callbacks to report thermal information
class MUsbThermalNotify
	{
public:
    /**
     * Called by thermal monitor plugin when there is any changes about thermal info
     *
     * @param aLastError The last error code detected
     * @param aValue The new value of thermal
     */
	virtual void UsbThermalStateChange(TInt aLastError, TInt aValue) = 0;
	};

/**
 *  This class is used by usbman server to register itself as an observer of 
 *  system thermal event.
 */  
class MUsbThermalPluginInterface
	{
public:	
	virtual void RegisterThermalObserver(MUsbThermalNotify& aThermalObserver) = 0;
	};
	
#endif //MUSBTHERMALNOTIFY_H
