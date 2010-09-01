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

#include "chargingstates.h"
#include <usb/usblogger.h>
#include "reenumerator.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBCHARGEStates");
#endif

// Charging plugin base state

// Empty virtual function implement to give a base of each state class. 
// A concrete state class can overlap them according to actual demand.
void TUsbBatteryChargingPluginStateBase::UsbServiceStateChange(TInt aLastError,
    TUsbServiceState aOldState, TUsbServiceState aNewState)
    {
    LOG_FUNC
    
    (void)aLastError;
    (void)aOldState;
    (void)aNewState;
    
    // Not use
    }

void TUsbBatteryChargingPluginStateBase::UsbDeviceStateChange(TInt aLastError,
    TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC
    
    (void)aLastError;
    (void)aOldState;
    (void)aNewState;
    }

void TUsbBatteryChargingPluginStateBase::HandleRepositoryValueChangedL(
    const TUid& aRepository, TUint aId, TInt aVal)
    {
    LOG_FUNC
    
    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateBase::HandleRepositoryValueChangedL aRepository = 0x%08x, aId = %d, aVal = %d"), aRepository, aId, aVal);
    LOGTEXT3(_L8("Plugin State = %d, Device State = %d"), iParent.iPluginState, iParent.iDeviceState);
    
    if ((aRepository == KUsbBatteryChargingCentralRepositoryUid) &&
            (aId == KUsbBatteryChargingKeyEnabledUserSetting))
        {
        iParent.iUserSetting = (TUsbBatteryChargingUserSetting)aVal;

        if (iParent.iUserSetting == EUsbBatteryChargingUserSettingDisabled)
            {
            if(iParent.iPluginState == EPluginStateCharging)
                {
                iParent.StopCharging();
                
                // Push EPluginStateIdleNegotiated state to recover state
                iParent.PushRecoverState(EPluginStateIdleNegotiated);
                }
            else
                {
                // Push current state to recover state
                iParent.PushRecoverState(iParent.iPluginState);
                }
            
            iParent.SetState(EPluginStateUserDisabled);
            }
        }
    }
    
void TUsbBatteryChargingPluginStateBase::DeviceStateTimeout()
    {
    LOG_FUNC
    LOGTEXT4(_L8("Time: %d Plugin State = %d, Device State = %d"), User::NTickCount(), iParent.iPluginState, iParent.iDeviceState);
    
    iParent.iDeviceReEnumerator->Cancel(); // cancel re-enumeration AO
    
    if(iParent.iUserSetting) // User allow charging already and not in negotiating process...
        {
        // Should not happen !!! Otherwise, something wrong!!!
        iParent.SetState(EPluginStateIdle);
        }
    }

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void TUsbBatteryChargingPluginStateBase::MpsoIdPinStateChanged(TInt aValue)
    {
    LOG_FUNC
    
    LOGTEXT2(_L8("IdPinState changed => %d"), aValue);
    
    // Disable charging here when IdPin is present
    // When IdPin disappears (i.e. the phone becomes B-Device), all necessary step are performed 
    // in UsbDeviceStateChange() method

    iParent.iIdPinState = aValue;
    
    // For all other states besides EPluginStateUserDisabled
    switch(aValue)
        {
        case EUsbBatteryChargingIdPinARole:
            if (iParent.iPluginState == EPluginStateCharging)
                {
                iParent.StopCharging();
                }

            TRAP_IGNORE(iParent.SetInitialConfigurationL());
            iParent.SetState(EPluginStateBEndedCableNotPresent);
            
            return;

        case EUsbBatteryChargingIdPinBRole:
            iParent.SetState(EPluginStateIdle);
            break;

        default:
            if (iParent.iPluginState == EPluginStateCharging)
                {
                iParent.StopCharging();
                }     
            iParent.SetState(EPluginStateIdle);
            break;
        }
    }

void TUsbBatteryChargingPluginStateBase::MpsoOtgStateChangedL(TUsbOtgState aNewState)
    {
    LOG_FUNC
    
    iParent.iOtgState = aNewState;
    
    // Not use currently
    }
#endif

void TUsbBatteryChargingPluginStateBase::MpsoVBusStateChanged(TInt aNewState)
    {
    LOG_FUNC
    
    if (aNewState == iParent.iVBusState)
        {
        LOGTEXT2(_L8("Receive VBus State Change notification without any state change: aNewState = %d"), aNewState);
        return;//should not happen??
        }

    LOGTEXT3(_L8("VBusState changed from %d to %d"), iParent.iVBusState, aNewState);
    
    iParent.iVBusState = aNewState;
    if (aNewState == 0) // VBus drop down - we have disconnected from host
        {
        if (iParent.iPluginState == EPluginStateCharging)
            {
            iParent.StopCharging();
            }
        iParent.SetState(EPluginStateIdle);
        }
    
    // The handling of VBus on will be down in DeviceStateChanged implicitly
    }


TUsbBatteryChargingPluginStateBase::TUsbBatteryChargingPluginStateBase (
        CUsbBatteryChargingPlugin& aParentStateMachine ): 
        iParent(aParentStateMachine)
    {
    LOG_FUNC
    }

        
// Charging plugin idle state

TUsbBatteryChargingPluginStateIdle::TUsbBatteryChargingPluginStateIdle (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };

void TUsbBatteryChargingPluginStateIdle::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC

    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateIdle::UsbDeviceStateChange LastError = %d, aOldState = %d, aNewState = %d"), aLastError, aOldState, aNewState);
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);
    
    switch (iParent.iDeviceState)
        {
        case EUsbDeviceStateAddress:
            {
            if (iParent.iUserSetting)
                {
                if (iParent.IsUsbChargingPossible())
                    {
                    iParent.NegotiateChargingCurrent();
                    }
                }
            else
                {
                iParent.SetState(EPluginStateUserDisabled);
                }
            }
            break;
        }
    }
    
    
// Charging plugin current negotiating state

TUsbBatteryChargingPluginStateCurrentNegotiating::TUsbBatteryChargingPluginStateCurrentNegotiating (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };
    
void TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC

    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange LastError = %d, aOldState = %d, aNewState = %d"), aLastError, aOldState, aNewState);
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);
    
    switch(iParent.iDeviceState)
        {
        case EUsbDeviceStateConfigured:
            if (iParent.IsUsbChargingPossible())
                {
                iParent.iDeviceStateTimer->Cancel();
                
                LOGTEXT2(_L8("iParent.iAvailableMilliAmps = %d"),iParent.iAvailableMilliAmps);
                iParent.iAvailableMilliAmps = iParent.iRequestedCurrentValue;
                
                if(0 != iParent.iRequestedCurrentValue)
                    {
                    // A non-zero value was accepted by host, charging 
                    // can be performed now.
                    iParent.StartCharging(iParent.iAvailableMilliAmps);                     
                    LOGTEXT2(_L8("PluginState => EPluginStateCharging(%d)"),iParent.iPluginState);
                    iParent.SetNegotiatedCurrent(iParent.iAvailableMilliAmps);
                    }
                else
                    {
                    // Host can only accept 0 charging current
                    // No way to do charging
                    iParent.SetState(EPluginStateNoValidCurrent);
                    LOGTEXT2(_L8("No more current value to try, iPluginState turned to %d"), iParent.iPluginState);
                    }
                }
            
            break;
        
        // If no configured received, there must be a timeout
        // caught by the iDeviceStateTimer, and it will try next value or send state to  
        // EPluginStateNoValidCurrent, so don't worry that we omit something important :-)
            
            
        default:
            break;
        }
    }

void TUsbBatteryChargingPluginStateCurrentNegotiating::DeviceStateTimeout()
    {
    LOG_FUNC
    LOGTEXT4(_L8("Time: %d Plugin State = %d, Device State = %d"), User::NTickCount(), iParent.iPluginState, iParent.iDeviceState);
    
    iParent.iDeviceReEnumerator->Cancel(); // cancel re-enumeration AO
    
    if(iParent.iRequestedCurrentValue != 0)
        {
        // If there are more value to try ...
        iParent.NegotiateChargingCurrent();
        }
    else
        {
        // The Host doesn't accept 0ma power request?
        // Assume it will never happens.
        iParent.SetState(EPluginStateNoValidCurrent);
        }
    }


// Charging plugin charing state

    
TUsbBatteryChargingPluginStateCharging::TUsbBatteryChargingPluginStateCharging (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    }

void TUsbBatteryChargingPluginStateCharging::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC

    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateCharging::UsbDeviceStateChange LastError = %d, aOldState = %d, aNewState = %d"), aLastError, aOldState, aNewState);
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);

    switch(iParent.iDeviceState)
        {
        case EUsbDeviceStateConfigured:
            break; // I think this can not happen at all but in case ...
            
        case EUsbDeviceStateAttached:
        case EUsbDeviceStatePowered:
        case EUsbDeviceStateDefault:
        case EUsbDeviceStateAddress:
        case EUsbDeviceStateSuspended:
            {
            // wait until configured
            iParent.StopCharging();
            iParent.SetState(EPluginStateIdleNegotiated);
            }
            break;
                            
        default:
            break;
        }
    }

// Charging plugin negotiated fail state
    
    
TUsbBatteryChargingPluginStateNoValidCurrent::TUsbBatteryChargingPluginStateNoValidCurrent (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };

    
// Charging plugin idle negotiated state

TUsbBatteryChargingPluginStateIdleNegotiated::TUsbBatteryChargingPluginStateIdleNegotiated (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };

void TUsbBatteryChargingPluginStateIdleNegotiated::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC

    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateIdleNegotiated::UsbDeviceStateChange LastError = %d, aOldState = %d, aNewState = %d"), aLastError, aOldState, aNewState);
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);

    switch(iParent.iDeviceState)
        {
        case EUsbDeviceStateConfigured:
            {
            // wait until configured
            if (iParent.IsUsbChargingPossible())
            	{
                iParent.StartCharging(iParent.iAvailableMilliAmps);
                }
            }
            break;

        default:
            break;
        }
    }
 
// Charging plugin user disabled state
    
TUsbBatteryChargingPluginStateUserDisabled::TUsbBatteryChargingPluginStateUserDisabled (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };


void TUsbBatteryChargingPluginStateUserDisabled::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC

    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateUserDisabled::UsbDeviceStateChange LastError = %d, aOldState = %d, aNewState = %d"), aLastError, aOldState, aNewState);
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);
    }

void TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL(
    const TUid& aRepository, TUint aId, TInt aVal)
    {
    LOG_FUNC
    
    LOGTEXT4(_L8(">>TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL aRepository = 0x%08x, aId = %d, aVal = %d"), aRepository, aId, aVal);
    LOGTEXT3(_L8("Plugin State = %d, Device State = %d"), iParent.iPluginState, iParent.iDeviceState);
    
    if ((aRepository == KUsbBatteryChargingCentralRepositoryUid) &&
            (aId == KUsbBatteryChargingKeyEnabledUserSetting))
        {
        iParent.iUserSetting = (TUsbBatteryChargingUserSetting)aVal;

        if (iParent.iUserSetting == EUsbBatteryChargingUserSettingEnabled)
            {
            // EPluginStateUserDisabled must be the current state
            iParent.PopRecoverState();
            if ((iParent.iPluginState == EPluginStateIdleNegotiated)
                    && (iParent.iDeviceState == EUsbDeviceStateConfigured))
                {
                iParent.StartCharging(iParent.iAvailableMilliAmps); // Go to charing state implicitly
                }
            LOGTEXT2(_L8("PluginState => %d"), iParent.iPluginState);
            }
        }
    }

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void TUsbBatteryChargingPluginStateUserDisabled::MpsoIdPinStateChanged(TInt aValue)
    {
    LOG_FUNC
    
    LOGTEXT2(_L8("IdPinState changed => %d"), aValue);
    
    // Disable charging here when IdPin is present
    // When IdPin disappears (i.e. the phone becomes B-Device), all necessary step are performed 
    // in UsbDeviceStateChange() method

    iParent.iIdPinState = aValue;
    
    switch(aValue)
        {
        case EUsbBatteryChargingIdPinARole:
            TRAP_IGNORE(iParent.SetInitialConfigurationL());
            iParent.PushRecoverState(EPluginStateBEndedCableNotPresent);
            
            return;

        case EUsbBatteryChargingIdPinBRole:
            iParent.PushRecoverState(EPluginStateIdle);
            break;

        default:     
            iParent.SetState(EPluginStateIdle);
            break;
        }
    }

#endif     
 
void TUsbBatteryChargingPluginStateUserDisabled::MpsoVBusStateChanged(TInt aNewState)
    {
    LOG_FUNC
    
    if (aNewState == iParent.iVBusState)
        {
        LOGTEXT2(_L8("Receive VBus State Change notification without any state change: aNewState = %d"), aNewState);
        return;
        }

    LOGTEXT3(_L8("VBusState changed from %d to %d"), iParent.iVBusState, aNewState);
    
    iParent.iVBusState = aNewState;
    if (aNewState == 0) // VBus drop down - we have disconnected from host
        {
        iParent.iRequestedCurrentValue = 0;
        iParent.iCurrentIndexRequested = 0;
        iParent.iAvailableMilliAmps = 0;
        
        iParent.iPluginStateToRecovery = EPluginStateIdle;
        }
    
    // The handling of VBus on will be down in DeviceStateChanged implicitly
    }


// Charging plugin A-role state
    
TUsbBatteryChargingPluginStateBEndedCableNotPresent::TUsbBatteryChargingPluginStateBEndedCableNotPresent (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    LOG_FUNC
    };
    
    

