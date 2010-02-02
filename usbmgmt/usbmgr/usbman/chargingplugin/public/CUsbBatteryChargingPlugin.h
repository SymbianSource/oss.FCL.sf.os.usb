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

/** @file
@internalComponent
*/

#ifndef USBBATTERYCHARGINGPLUGIN_H
#define USBBATTERYCHARGINGPLUGIN_H

#include <e32base.h>

#ifdef __CHARGING_PLUGIN_TEST_CODE__
#include <dummyldd.h>
#define __D32USBC_H__ // ensure that d32usbc is ignored
#define private protected
#else
#include <d32usbc.h>
#endif

#include <cusbmanextensionplugin.h>
#include "usbbatterycharging.h"
#include <musbdevicenotify.h>
#include "devicestatetimer.h"
#include "repositorynotifier.h"
#include "motgobserver.h"

class CUsbChargingReEnumerator;
class CUsbBatteryChargingLicenseeHooks;

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
class CIdPinWatcher;
class COtgStateWatcher;
#endif

class CVBusWatcher;

enum TUsbBatteryChargingPanic
    {
    EUsbBatteryChargingPanicBadUserSetting = 0,
    EUsbBatteryChargingPanicBadPluginState = 1,
    EUsbBatteryChargingPanicUnexpectedPluginState = 2,
    EUsbBatteryChargingPanicBadDeviceState = 3,
    EUsbBatteryChargingPanicBadOtgState = 4,
    EUsbBatteryChargingPanicBadVBusState = 5,
    EUsbBatteryChargingPanicBadCharingCurrentNegotiation = 6
    };

// For host OTG enabled charging plug-in
class MUsbBatteryChargingPluginInterface : public MUsbDeviceNotify,
    public MUsbChargingRepositoryObserver,
    public MUsbChargingDeviceStateTimerObserver, public MOtgPropertiesObserver
    {
public:  // from MUsbDeviceNotify
    virtual void UsbServiceStateChange (TInt aLastError,
        TUsbServiceState aOldState, TUsbServiceState aNewState) = 0;
    virtual void UsbDeviceStateChange (TInt aLastError,
        TUsbDeviceState aOldState, TUsbDeviceState aNewState) = 0;

public: // from MUsbChargingRepositoryObserver
    virtual void HandleRepositoryValueChangedL(const TUid& aRepository, TUint aId, TInt aVal) = 0;
    
public: // from MUsbChargingDeviceStateTimerObserver
    virtual void DeviceStateTimeout() = 0;

public: // from MOtgPropertiesObserver
// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV 
    virtual void MpsoIdPinStateChanged(TInt aValue) = 0;
    virtual void MpsoOtgStateChangedL(TUsbOtgState aNewState) = 0;
#endif    
    virtual void MpsoVBusStateChanged(TInt aNewState) = 0;
    };

class TUsbBatteryChargingPluginStateBase;
class TUsbBatteryChargingPluginStateIdle;
class TUsbBatteryChargingPluginStateIdleUserDisabled;
class TUsbBatteryChargingPluginStateNoValidcurrent;
class TUsbBatteryChargingPluginStateCurrentNegotiating;
class TUsbBatteryChargingPluginStateCharging;
class TUsbBatteryChargingPluginStateIdelNegotiated;
class TUsbBatteryChargingPluginStateBEndedCableNotPresent;

enum TUsbChargingPluginState
    {
    // The default first state of this plugin
    EPluginStateIdle = 0,
    
    // Start to negotiate with A device about the charging current
    // re-enumration is executed 
    EPluginStateCurrentNegotiating,
    
    // Negotiation failed
    EPluginStateNoValidCurrent,
    
    // The only that indicate charging is going on 
    EPluginStateCharging,
    
    // charging is stopped for some reason, but negotiation is done already
    EPluginStateIdleNegotiated,
    
    // User Disabled "charging from usb" functionality
    EPluginStateUserDisabled,

    // Deivce is connect with A end cable, so, no way to do a charging to itself 
    EPluginStateBEndedCableNotPresent,
        
    // State counter
    EPluginStateCount
    };


class CUsbBatteryChargingPlugin : public CUsbmanExtensionPlugin,
    private MUsbBatteryChargingPluginInterface 
    {
    friend class TUsbBatteryChargingPluginStateBase;
    friend class TUsbBatteryChargingPluginStateIdle;
    friend class TUsbBatteryChargingPluginStateUserDisabled;
    friend class TUsbBatteryChargingPluginStateNoValidCurrent;
    friend class TUsbBatteryChargingPluginStateCurrentNegotiating;
    friend class TUsbBatteryChargingPluginStateCharging;
    friend class TUsbBatteryChargingPluginStateIdleNegotiated;
    friend class TUsbBatteryChargingPluginStateBEndedCableNotPresent;
    
public:
    static CUsbBatteryChargingPlugin* NewL(MUsbmanExtensionPluginObserver& aObserver);
    ~CUsbBatteryChargingPlugin();

private: // from CUsbmanExtensionPlugin
    TAny* GetInterface(TUid aUid);
    
private:  // from MUsbBatteryChargingPluginInterface
    // from MUsbDeviceNotify
    void UsbServiceStateChange (TInt aLastError,
        TUsbServiceState aOldState, TUsbServiceState aNewState);
    void UsbDeviceStateChange (TInt aLastError,
        TUsbDeviceState aOldState, TUsbDeviceState aNewState);

    // from MUsbChargingRepositoryObserver
    void HandleRepositoryValueChangedL(const TUid& aRepository, TUint aId, TInt aVal);
    
    // from MUsbChargingDeviceStateTimerObserver
    void DeviceStateTimeout();

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV 
    // from MOtgPropertiesObserver
    void MpsoIdPinStateChanged(TInt aValue);
    void MpsoOtgStateChangedL(TUsbOtgState aNewState);
#endif
    
    void MpsoVBusStateChanged(TInt aNewState);
private:
    CUsbBatteryChargingPlugin(MUsbmanExtensionPluginObserver& aObserver);
    void ConstructL();
    void Panic(TUsbBatteryChargingPanic aPanic);

private:
    void StartCharging(TUint aMilliAmps);
    void StopCharging();
    
    void SetNegotiatedCurrent(TUint aMilliAmps);
    void ReadCurrentRequestValuesL();
    
    void NegotiateChargingCurrent();
    void NegotiateNextCurrentValueL();
    void RequestCurrentL(TUint aMilliAmps);
    
    void ResetPlugin();
            
    void SetInitialConfigurationL();
    
    TBool IsUsbChargingPossible();
    
    void LogStateText(TUsbDeviceState aState);
    void PushRecoverState(TUsbChargingPluginState aRecoverState);
    TUsbChargingPluginState PopRecoverState();
    
    TUsbChargingPluginState SetState(TUsbChargingPluginState aState);
    
private: // owned
    RDevUsbcClient& iLdd;
       
    // user allow usb charging function already?
    TUsbBatteryChargingUserSetting iUserSetting;
    
    // More than one value will be tried by the negotiation process to 
    // aquire a as larger current value as possible for charging
    RArray<TInt> iCurrentValues;
    
    //value of requested current set to descriptor for negotiation
    TInt iRequestedCurrentValue;
    
    //index of requested value
    TInt iCurrentIndexRequested;
    
    //value of negotiated current
    TInt iAvailableMilliAmps;   

    CUsbChargingDeviceStateTimer*   iDeviceStateTimer;
    CUsbChargingRepositoryNotifier* iRepositoryNotifier;
    CUsbChargingReEnumerator*       iDeviceReEnumerator;

    CUsbBatteryChargingLicenseeHooks* iLicenseeHooks;

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV     
    //Outer state watchers
    COtgStateWatcher *iOtgStateWatcher;
    CIdPinWatcher *iIdPinWatcher;
    //Outer states
    TInt iIdPinState;
    TUsbOtgState iOtgState;
#endif    
    
    TInt iVBusState;
    CVBusWatcher *iVBusWatcher;
    TUsbDeviceState iDeviceState;
    
    // Plug-in States
    TUsbChargingPluginState iPluginState; // Current state machine status
    TUsbChargingPluginState iPluginStateToRecovery; // The recover state when user enable USB Charging
    TUsbBatteryChargingPluginStateBase* iCurrentState; // Owned
    TUsbBatteryChargingPluginStateBase* iPluginStates[EPluginStateCount]; // State implementations
    };

#endif // USBBATTERYCHARGINGPLUGIN_H
