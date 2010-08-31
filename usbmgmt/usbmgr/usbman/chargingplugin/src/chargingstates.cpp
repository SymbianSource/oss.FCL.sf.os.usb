/*
* Copyright (c) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include <usb/usblogger.h>
#include "chargingstates.h"
#include "reenumerator.h"
#include "OstTraceDefinitions.h"
#ifdef OST_TRACE_COMPILER_IN_USE
#include "chargingstatesTraces.h"
#endif



// Charging plugin base state

// Empty virtual function implement to give a base of each state class. 
// A concrete state class can overlap them according to actual demand.
void TUsbBatteryChargingPluginStateBase::UsbServiceStateChange(TInt aLastError,
    TUsbServiceState aOldState, TUsbServiceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_USBSERVICESTATECHANGE_ENTRY );
    
    (void)aLastError;
    (void)aOldState;
    (void)aNewState;
    
    // Not use
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_USBSERVICESTATECHANGE_EXIT );
    }

void TUsbBatteryChargingPluginStateBase::UsbDeviceStateChange(TInt aLastError,
    TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_USBDEVICESTATECHANGE_ENTRY );
    
    (void)aLastError;
    (void)aOldState;
    (void)aNewState;
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_USBDEVICESTATECHANGE_EXIT );
    }

void TUsbBatteryChargingPluginStateBase::HandleRepositoryValueChangedL(
    const TUid& aRepository, TUint aId, TInt aVal)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_HANDLEREPOSITORYVALUECHANGEDL_ENTRY );
    
    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_HANDLEREPOSITORYVALUECHANGEDL, 
            "TUsbBatteryChargingPluginStateBase::HandleRepositoryValueChangedL;aRepository = 0x%08x;aId=%d;aVal=%d", 
            aRepository.iUid, aId, (TInt32)aVal );
    OstTraceExt2( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_HANDLEREPOSITORYVALUECHANGEDL_DUP1, 
            "TUsbBatteryChargingPluginStateBase::HandleRepositoryValueChangedL;Plugin State = %d, Device State = %d",
            iParent.iPluginState, iParent.iDeviceState );
    
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_HANDLEREPOSITORYVALUECHANGEDL_EXIT );
    }
    
void TUsbBatteryChargingPluginStateBase::DeviceStateTimeout()
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_DEVICESTATETIMEOUT_ENTRY );
    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_DEVICESTATETIMEOUT, "TUsbBatteryChargingPluginStateBase::DeviceStateTimeout;Time: %u Plugin State = %d, Device State = %d", User::NTickCount(), (TInt32)iParent.iPluginState, (TInt32)iParent.iDeviceState );
    
    iParent.iDeviceReEnumerator->Cancel(); // cancel re-enumeration AO
    
    if(iParent.iUserSetting) // User allow charging already and not in negotiating process...
        {
        // Should not happen !!! Otherwise, something wrong!!!
        iParent.SetState(EPluginStateIdle);
        }
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_DEVICESTATETIMEOUT_EXIT );
    }

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void TUsbBatteryChargingPluginStateBase::MpsoIdPinStateChanged(TInt aValue)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOIDPINSTATECHANGED_ENTRY );

    OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOIDPINSTATECHANGED, "TUsbBatteryChargingPluginStateBase::MpsoIdPinStateChanged;IdPinState changed => %d", aValue );
    
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
            
            OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOIDPINSTATECHANGED_EXIT );
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOIDPINSTATECHANGED_EXIT_DUP1 );
    }

void TUsbBatteryChargingPluginStateBase::MpsoOtgStateChangedL(TUsbOtgState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOOTGSTATECHANGEDL_ENTRY );
    
    iParent.iOtgState = aNewState;
    
    // Not use currently
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOOTGSTATECHANGEDL_EXIT );
    }
#endif

void TUsbBatteryChargingPluginStateBase::MpsoVBusStateChanged(TInt aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOVBUSSTATECHANGED_ENTRY );
    
    if (aNewState == iParent.iVBusState)
        {
        OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOVBUSSTATECHANGED, "TUsbBatteryChargingPluginStateBase::MpsoVBusStateChanged;Receive VBus State Change notification without any state change: aNewState = %d", aNewState );
        OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOVBUSSTATECHANGED_EXIT );
        return;//should not happen??
        }

    OstTraceExt2( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOVBUSSTATECHANGED_DUP1, "TUsbBatteryChargingPluginStateBase::MpsoVBusStateChanged;VBusState changed from %d to %d", iParent.iVBusState, aNewState );
    
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_MPSOVBUSSTATECHANGED_EXIT_DUP1 );
    }


TUsbBatteryChargingPluginStateBase::TUsbBatteryChargingPluginStateBase (
        CUsbBatteryChargingPlugin& aParentStateMachine ): 
        iParent(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_TUSBBATTERYCHARGINGPLUGINSTATEBASE_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEBASE_TUSBBATTERYCHARGINGPLUGINSTATEBASE_CONS_EXIT );
    }

        
// Charging plugin idle state

TUsbBatteryChargingPluginStateIdle::TUsbBatteryChargingPluginStateIdle (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_CONS_EXIT );
    };

void TUsbBatteryChargingPluginStateIdle::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_USBDEVICESTATECHANGE_ENTRY );

    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_USBDEVICESTATECHANGE, "TUsbBatteryChargingPluginStateIdle::UsbDeviceStateChange;aLastError=%d;aOldState=%d;aNewState=%d", aLastError, aOldState, aNewState );
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLE_USBDEVICESTATECHANGE_EXIT );
    }
    
    
// Charging plugin current negotiating state

TUsbBatteryChargingPluginStateCurrentNegotiating::TUsbBatteryChargingPluginStateCurrentNegotiating (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_CONS_EXIT );
    };
    
void TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE_ENTRY );

    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE, "TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange;aLastError=%d;aOldState=%d;aNewState=%d", aLastError, aOldState, aNewState );
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
                
                OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE_DUP1, "TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange;iParent.iAvailableMilliAmps=%d", iParent.iAvailableMilliAmps );
                iParent.iAvailableMilliAmps = iParent.iRequestedCurrentValue;
                
                if(0 != iParent.iRequestedCurrentValue)
                    {
                    // A non-zero value was accepted by host, charging 
                    // can be performed now.
                    iParent.StartCharging(iParent.iAvailableMilliAmps);                     
                    OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE_DUP2, "TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange;PluginState => EPluginStateCharging(%d)", iParent.iPluginState );
                    iParent.SetNegotiatedCurrent(iParent.iAvailableMilliAmps);
                    }
                else
                    {
                    // Host can only accept 0 charging current
                    // No way to do charging
                    iParent.SetState(EPluginStateNoValidCurrent);
                    OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE_DUP3, "TUsbBatteryChargingPluginStateCurrentNegotiating::UsbDeviceStateChange;No more current value to try, iPluginState turned to %d", iParent.iPluginState );
                    }
                }
            
            break;
        
        // If no configured received, there must be a timeout
        // caught by the iDeviceStateTimer, and it will try next value or send state to  
        // EPluginStateNoValidCurrent, so don't worry that we omit something important :-)
            
            
        default:
            break;
        }
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_USBDEVICESTATECHANGE_EXIT );
    }

void TUsbBatteryChargingPluginStateCurrentNegotiating::DeviceStateTimeout()
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_DEVICESTATETIMEOUT_ENTRY );
    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_DEVICESTATETIMEOUT, "TUsbBatteryChargingPluginStateCurrentNegotiating::DeviceStateTimeout;Time: %d Plugin State = %d, Device State = %d", User::NTickCount(), (TInt32)iParent.iPluginState, (TInt32)iParent.iDeviceState );
    
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATECURRENTNEGOTIATING_DEVICESTATETIMEOUT_EXIT );
    }


// Charging plugin charing state

    
TUsbBatteryChargingPluginStateCharging::TUsbBatteryChargingPluginStateCharging (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_CONS_EXIT );
    }

void TUsbBatteryChargingPluginStateCharging::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_USBDEVICESTATECHANGE_ENTRY );

    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_USBDEVICESTATECHANGE, "TUsbBatteryChargingPluginStateCharging::UsbDeviceStateChange;aLastError=%d;aOldState=%d;aNewState=%d", aLastError, aOldState, aNewState );
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATECHARGING_USBDEVICESTATECHANGE_EXIT );
    }

// Charging plugin negotiated fail state
    
    
TUsbBatteryChargingPluginStateNoValidCurrent::TUsbBatteryChargingPluginStateNoValidCurrent (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATENOVALIDCURRENT_TUSBBATTERYCHARGINGPLUGINSTATENOVALIDCURRENT_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATENOVALIDCURRENT_TUSBBATTERYCHARGINGPLUGINSTATENOVALIDCURRENT_CONS_EXIT );
    };

    
// Charging plugin idle negotiated state

TUsbBatteryChargingPluginStateIdleNegotiated::TUsbBatteryChargingPluginStateIdleNegotiated (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_CONS_EXIT );
    };

void TUsbBatteryChargingPluginStateIdleNegotiated::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_USBDEVICESTATECHANGE_ENTRY );

    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_USBDEVICESTATECHANGE, "TUsbBatteryChargingPluginStateIdleNegotiated::UsbDeviceStateChange;aLastError=%d;aOldState=%d;aNewState=%d", aLastError, aOldState, aNewState );
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
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEIDLENEGOTIATED_USBDEVICESTATECHANGE_EXIT );
    }
 
// Charging plugin user disabled state
    
TUsbBatteryChargingPluginStateUserDisabled::TUsbBatteryChargingPluginStateUserDisabled (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_CONS_ENTRY );
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_CONS_EXIT );
    };


void TUsbBatteryChargingPluginStateUserDisabled::UsbDeviceStateChange(
        TInt aLastError, TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_USBDEVICESTATECHANGE_ENTRY );

    OstTraceExt3( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_USBDEVICESTATECHANGE, "TUsbBatteryChargingPluginStateUserDisabled::UsbDeviceStateChange;aLastError=%d;aOldState=%d;aNewState=%d", aLastError, aOldState, aNewState );
    (void)aLastError;
    (void)aOldState;
    iParent.iDeviceState = aNewState;
    iParent.LogStateText(aNewState);
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_USBDEVICESTATECHANGE_EXIT );
    }

void TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL(
    const TUid& aRepository, TUint aId, TInt aVal)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL_ENTRY );
    
    OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL, "TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL;aRepository = 0x%08x", aRepository.iUid );
    OstTraceExt2( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL_DUP1, "TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL;aId=%d;aVal=%d", aId, aVal );
    OstTraceExt2( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL_DUP2, "TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL;iParent.iPluginState=%d;iParent.iDeviceState=%d", iParent.iPluginState, iParent.iDeviceState );
    
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
            OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL_DUP3, "TUsbBatteryChargingPluginStateUserDisabled::HandleRepositoryValueChangedL;PluginState => %d", iParent.iPluginState );
            }
        }
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_HANDLEREPOSITORYVALUECHANGEDL_EXIT );
    }

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void TUsbBatteryChargingPluginStateUserDisabled::MpsoIdPinStateChanged(TInt aValue)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOIDPINSTATECHANGED_ENTRY );
    
    OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOIDPINSTATECHANGED, "TUsbBatteryChargingPluginStateUserDisabled::MpsoIdPinStateChanged;IdPinState changed => %d", aValue );
    
    // Disable charging here when IdPin is present
    // When IdPin disappears (i.e. the phone becomes B-Device), all necessary step are performed 
    // in UsbDeviceStateChange() method

    iParent.iIdPinState = aValue;
    
    switch(aValue)
        {
        case EUsbBatteryChargingIdPinARole:
            TRAP_IGNORE(iParent.SetInitialConfigurationL());
            iParent.PushRecoverState(EPluginStateBEndedCableNotPresent);
            
            OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOIDPINSTATECHANGED_EXIT );
            return;

        case EUsbBatteryChargingIdPinBRole:
            iParent.PushRecoverState(EPluginStateIdle);
            break;

        default:     
            iParent.SetState(EPluginStateIdle);
            break;
        }
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOIDPINSTATECHANGED_EXIT_DUP1 );
    }

#endif     
 
void TUsbBatteryChargingPluginStateUserDisabled::MpsoVBusStateChanged(TInt aNewState)
    {
    OstTraceFunctionEntry0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOVBUSSTATECHANGED_ENTRY );
    
    if (aNewState == iParent.iVBusState)
        {
        OstTrace1( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOVBUSSTATECHANGED, "TUsbBatteryChargingPluginStateUserDisabled::MpsoVBusStateChanged;Receive VBus State Change notification without any state change: aNewState = %d", aNewState );
        OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOVBUSSTATECHANGED_EXIT );
        return;
        }

    OstTraceExt2( TRACE_NORMAL, REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOVBUSSTATECHANGED_DUP1, "TUsbBatteryChargingPluginStateUserDisabled::MpsoVBusStateChanged;VBusState changed from %d to %d", iParent.iVBusState, aNewState );
    
    iParent.iVBusState = aNewState;
    if (aNewState == 0) // VBus drop down - we have disconnected from host
        {
        iParent.iRequestedCurrentValue = 0;
        iParent.iCurrentIndexRequested = 0;
        iParent.iAvailableMilliAmps = 0;
        
        iParent.iPluginStateToRecovery = EPluginStateIdle;
        }
    
    // The handling of VBus on will be down in DeviceStateChanged implicitly
    OstTraceFunctionExit0( REF_TUSBBATTERYCHARGINGPLUGINSTATEUSERDISABLED_MPSOVBUSSTATECHANGED_EXIT_DUP1 );
    }


// Charging plugin A-role state
    
TUsbBatteryChargingPluginStateBEndedCableNotPresent::TUsbBatteryChargingPluginStateBEndedCableNotPresent (
        CUsbBatteryChargingPlugin& aParentStateMachine ) :
    TUsbBatteryChargingPluginStateBase(aParentStateMachine)
    {
    OstTraceFunctionEntry0( RES_TUSBBATTERYCHARGINGPLUGINSTATEBENDEDCABLENOTPRESENT_TUSBBATTERYCHARGINGPLUGINSTATEBENDEDCABLENOTPRESENT_CONS_ENTRY );
    OstTraceFunctionExit0( RES_TUSBBATTERYCHARGINGPLUGINSTATEBENDEDCABLENOTPRESENT_TUSBBATTERYCHARGINGPLUGINSTATEBENDEDCABLENOTPRESENT_CONS_EXIT );
    };
    
    

