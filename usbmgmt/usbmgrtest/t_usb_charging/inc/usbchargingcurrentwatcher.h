/*
* Copyright (c) 2006-2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef __USBCHARGINGCURRENTWATCHER_H__
#define __USBCHARGINGCURRENTWATCHER_H__

#include <e32property.h>
#include <e32cmn.h>
#include "usbchargingarmtest.h"
#include "testbase.h"

/**
*/
class CUsbChargingCurrentWatcher : public CActive
    {
public:
    static CUsbChargingCurrentWatcher* NewL(CUsbChargingArmTest& aUsbChargingArm);
    ~CUsbChargingCurrentWatcher();

private:
    CUsbChargingCurrentWatcher(CUsbChargingArmTest& aUsbChargingArm);
    void ConstructL();
    
    void DoCancel();
    void RunL();
    
    void PostAndNotifyCurrentL();

private:
    RProperty           iChargingCurrentProp;  
    CUsbChargingArmTest& iUsbChargingArm;
    };

#endif // __USBCHARGINGCURRENTWATCHER_H__
