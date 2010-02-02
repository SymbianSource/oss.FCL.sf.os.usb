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

/**
 @file
 @internalComponent
*/

#include <e32base.h>


class CUsbBatteryChargingLicenseeHooks : public CBase
	{
public:
	inline static CUsbBatteryChargingLicenseeHooks* NewL();
	inline ~CUsbBatteryChargingLicenseeHooks();

	inline void StartCharging(TUint aMilliAmps);
	inline void StopCharging();
private:
	inline CUsbBatteryChargingLicenseeHooks();
	inline void ConstructL();
	};


inline CUsbBatteryChargingLicenseeHooks* CUsbBatteryChargingLicenseeHooks::NewL()
	{
	CUsbBatteryChargingLicenseeHooks* self = new(ELeave) CUsbBatteryChargingLicenseeHooks;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
	}

inline CUsbBatteryChargingLicenseeHooks::~CUsbBatteryChargingLicenseeHooks()
	{
	}

inline void CUsbBatteryChargingLicenseeHooks::StartCharging(TUint /*aMilliAmps*/)
	{
	}

inline void CUsbBatteryChargingLicenseeHooks::StopCharging()
	{
	}

inline CUsbBatteryChargingLicenseeHooks::CUsbBatteryChargingLicenseeHooks()
	{
	}

inline void CUsbBatteryChargingLicenseeHooks::ConstructL()
	{
	}

