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
*
*/

/** @file
@internalComponent
*/

#ifndef USBBATTERYCHARGING_H
#define USBBATTERYCHARGING_H

#include <e32base.h>

// UID used for central respository
const TUid KUsbBatteryChargingCentralRepositoryUid = {0x10208DD7};	// UID3 for usbbatterychargingplugin

const TUid KPropertyUidUsbBatteryChargingCategory = {0x101fe1db};
const TUint KPropertyUidUsbBatteryChargingAvailableCurrent = 1; // current negotiated
const TUint KPropertyUidUsbBatteryChargingChargingCurrent = 2; // current for charging (i.e. depends on user setting)

const TUint KUsbBatteryChargingKeyEnabledUserSetting = 1;
const TUint KUsbBatteryChargingKeyNumberOfCurrentValues = 2;

const TUint KUsbBatteryChargingCurrentValuesOffset = 0x1000;

enum TUsbBatteryChargingUserSetting
	{
	EUsbBatteryChargingUserSettingDisabled = 0,
	EUsbBatteryChargingUserSettingEnabled,
	};

enum TUsbIdPinState
    {
    EUsbBatteryChargingIdPinBRole = 0,
    EUsbBatteryChargingIdPinARole,
    };

#endif // USBBATTERYCHARGING_H
