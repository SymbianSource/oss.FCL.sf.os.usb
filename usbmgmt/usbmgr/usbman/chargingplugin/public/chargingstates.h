/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
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
#ifndef CHARGINGSTATES_H
#define CHARGINGSTATES_H

#include "CUsbBatteryChargingPlugin.h"

class TUsbBatteryChargingPluginStateBase : public MUsbBatteryChargingPluginInterface
    {
    friend class CUsbBatteryChargingPlugin;
    
protected:  // from MUsbBatteryChargingPluginInterface
    // from MUsbDeviceNotify
    virtual void UsbServiceStateChange (TInt aLastError,
        TUsbServiceState aOldState, TUsbServiceState aNewState);
    virtual void UsbDeviceStateChange (TInt aLastError,
        TUsbDeviceState aOldState, TUsbDeviceState aNewState);

    // from MUsbChargingRepositoryObserver
    virtual void HandleRepositoryValueChangedL (const TUid& aRepository,
        TUint aId, TInt aVal);
    
    // from MUsbChargingDeviceStateTimerObserver
    virtual void DeviceStateTimeout();

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV          // For host OTG enabled charging plug-in    
    // from MOtgPropertiesObserver
    virtual void MpsoIdPinStateChanged(TInt aValue);
    virtual void MpsoOtgStateChangedL(TUsbOtgState aNewState);
#endif
    virtual void MpsoVBusStateChanged(TInt aNewState);
    
protected:
    TUsbBatteryChargingPluginStateBase(CUsbBatteryChargingPlugin& aParentStateMachine);
    
protected:
    CUsbBatteryChargingPlugin& iParent; // Charging state machine. Not Owned
    };

class TUsbBatteryChargingPluginStateIdle : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateIdle(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    
private:
    void UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState);
    };

class TUsbBatteryChargingPluginStateNoValidCurrent : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateNoValidCurrent(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    };

class TUsbBatteryChargingPluginStateCurrentNegotiating : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateCurrentNegotiating(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    
private:
    void UsbDeviceStateChange(TInt aLastError, TUsbDeviceState aOldState, 
            TUsbDeviceState aNewState);
    void DeviceStateTimeout();
    };

class TUsbBatteryChargingPluginStateCharging : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateCharging(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    
private:
    void UsbDeviceStateChange(TInt aLastError, TUsbDeviceState aOldState, 
            TUsbDeviceState aNewState);
    };

class TUsbBatteryChargingPluginStateIdleNegotiated : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateIdleNegotiated(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    
private:
    void UsbDeviceStateChange(TInt aLastError, TUsbDeviceState aOldState, 
            TUsbDeviceState aNewState);
    };

class TUsbBatteryChargingPluginStateUserDisabled : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateUserDisabled(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    
private:
    void UsbDeviceStateChange(TInt aLastError, TUsbDeviceState aOldState, 
            TUsbDeviceState aNewState);
    
    // from MUsbChargingRepositoryObserver
    void HandleRepositoryValueChangedL (const TUid& aRepository, 
            TUint aId, TInt aVal);
    
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV          // For host OTG enabled charging plug-in     
    void MpsoIdPinStateChanged(TInt aValue);
#endif
    void MpsoVBusStateChanged(TInt aNewState);
    };

class TUsbBatteryChargingPluginStateBEndedCableNotPresent : public TUsbBatteryChargingPluginStateBase
    {
public:
    TUsbBatteryChargingPluginStateBEndedCableNotPresent(
            CUsbBatteryChargingPlugin& aParentStateMachine);
    };

#endif // CHARGINGSTATES_H

// End of file
