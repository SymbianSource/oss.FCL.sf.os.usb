/*
* Copyright (c) 2005-2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "CUsbBatteryChargingPlugin.h"
#include <e32debug.h> 
#include <e32def.h>
#include <usb/usblogger.h>
#include <e32property.h>
#include <centralrepository.h>
#include <usbotgdefs.h>
#include <musbmanextensionpluginobserver.h>


#include "chargingstates.h"
#include "cusbbatterycharginglicenseehooks.h"
#include "reenumerator.h"

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV          // For host OTG enabled charging plug-in 
#include "idpinwatcher.h"
#include "otgstatewatcher.h"
#endif

#include "vbuswatcher.h"
#include "OstTraceDefinitions.h"
#ifdef OST_TRACE_COMPILER_IN_USE
#include "CUsbBatteryChargingPluginTraces.h"
#endif


static const TInt KUsbBatteryChargingConfigurationDescriptorCurrentOffset = 8; // see bMaxPower in section 9.6.3 of USB Spec 2.0
static const TInt KUsbBatteryChargingCurrentRequestTimeout = 3000000; // 3 seconds


/**
Factory function.
@return Ownership of a new CUsbBatteryChargingPlugin.
*/
CUsbBatteryChargingPlugin* CUsbBatteryChargingPlugin::NewL(MUsbmanExtensionPluginObserver& aObserver)
    {
    CUsbBatteryChargingPlugin* self = new(ELeave) CUsbBatteryChargingPlugin(aObserver);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

/**
Destructor.
*/
CUsbBatteryChargingPlugin::~CUsbBatteryChargingPlugin()
    {
    OstTraceFunctionEntry1( REF_CUSBBATTERYCHARGINGPLUGIN_CUSBBATTERYCHARGINGPLUGIN_DES_ENTRY, this );
    
    iCurrentValues.Close();
    delete iDeviceReEnumerator;
    delete iDeviceStateTimer;
    delete iRepositoryNotifier;
    delete iLicenseeHooks;
    
// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
    delete iIdPinWatcher;
    delete iOtgStateWatcher;
#endif
    
    delete iVBusWatcher;
    for (TInt index = 0; index < EPluginStateCount; index ++)
        {
        delete iPluginStates[index];
        iPluginStates[index] = NULL;
        }
    }

/**
Constructor.
*/
CUsbBatteryChargingPlugin::CUsbBatteryChargingPlugin(MUsbmanExtensionPluginObserver& aObserver)
:    CUsbmanExtensionPlugin(aObserver) , iLdd(Observer().DevUsbcClient())
    {
    }

/**
2nd-phase construction.
*/
void CUsbBatteryChargingPlugin::ConstructL()
    {
   OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_ENTRY );
   
    // Create state objects
    iPluginStates[EPluginStateIdle] = 
        new (ELeave) TUsbBatteryChargingPluginStateIdle(*this);
    iPluginStates[EPluginStateCurrentNegotiating] = 
        new (ELeave) TUsbBatteryChargingPluginStateCurrentNegotiating(*this);
    iPluginStates[EPluginStateCharging] = 
        new (ELeave) TUsbBatteryChargingPluginStateCharging(*this);
    iPluginStates[EPluginStateNoValidCurrent] = 
        new (ELeave) TUsbBatteryChargingPluginStateNoValidCurrent(*this);
    iPluginStates[EPluginStateIdleNegotiated] = 
        new (ELeave) TUsbBatteryChargingPluginStateIdleNegotiated(*this);
    iPluginStates[EPluginStateUserDisabled] = 
        new (ELeave) TUsbBatteryChargingPluginStateUserDisabled(*this);
    iPluginStates[EPluginStateBEndedCableNotPresent] = 
        new (ELeave) TUsbBatteryChargingPluginStateBEndedCableNotPresent(*this);

    // Set initial state to idle
    SetState(EPluginStateIdle);
    
    TInt err = RProperty::Define(KPropertyUidUsbBatteryChargingCategory,
                      KPropertyUidUsbBatteryChargingAvailableCurrent,
                      RProperty::EInt,
                      ECapabilityReadDeviceData,
                      ECapabilityCommDD);

    if(err == KErrNone || err == KErrAlreadyExists)
        {

        err = RProperty::Define(KPropertyUidUsbBatteryChargingCategory,
                          KPropertyUidUsbBatteryChargingChargingCurrent,
                          RProperty::EInt,
                          ECapabilityReadDeviceData,
                          ECapabilityCommDD);
        }
    else
        {
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL, "CUsbBatteryChargingPlugin::ConstructL;leave with error=%d", err );
        User::Leave(err );
        }

    if(err == KErrNone || err == KErrAlreadyExists)
        {
        err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                        KPropertyUidUsbBatteryChargingAvailableCurrent,
                                        0);
        }
    else
        {
        static_cast<void> (RProperty::Delete (
                KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingAvailableCurrent ));
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_DUP1, "CUsbBatteryChargingPlugin::ConstructL;leave with error=%d", err );
        User::Leave(err);
        }
    
    err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                        KPropertyUidUsbBatteryChargingChargingCurrent,
                                        0);

    if(err != KErrNone)
        {
        static_cast<void> (RProperty::Delete (
                KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingAvailableCurrent ));
        static_cast<void> (RProperty::Delete (
                KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingChargingCurrent ));
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_DUP2, "CUsbBatteryChargingPlugin::ConstructL;leave with error=%d", err );
        User::Leave(err );
        }
        
    iRepositoryNotifier = CUsbChargingRepositoryNotifier::NewL (*this,
            KUsbBatteryChargingCentralRepositoryUid,
            KUsbBatteryChargingKeyEnabledUserSetting );
    iDeviceStateTimer = CUsbChargingDeviceStateTimer::NewL(*this);

    iDeviceReEnumerator = CUsbChargingReEnumerator::NewL(iLdd);

    iPluginState = EPluginStateIdle;
    iPluginStateToRecovery = EPluginStateIdle;
    ReadCurrentRequestValuesL();
    iVBusWatcher = CVBusWatcher::NewL(this);
    iVBusState = iVBusWatcher->VBusState();
    
// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
    iOtgStateWatcher = COtgStateWatcher::NewL(this);
    iOtgState = iOtgStateWatcher->OtgState();
    iIdPinWatcher = CIdPinWatcher::NewL(this);
    TInt value = iIdPinWatcher->IdPinValue();
    iIdPinState = iIdPinWatcher->IdPinValue();
    if (iIdPinState == EUsbBatteryChargingIdPinBRole)
#else
    if (ETrue)
#endif
        {
#if !defined(__WINS__) && !defined(__CHARGING_PLUGIN_TEST_CODE__)
        SetInitialConfigurationL();
#endif
        }
    else
        {
        iPluginState = EPluginStateBEndedCableNotPresent;
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_DUP3, "CUsbBatteryChargingPlugin::ConstructL;PluginState => EPluginStateADevice(%d)", iPluginState );
        }

    Observer().RegisterStateObserverL(*this);

    iLicenseeHooks = CUsbBatteryChargingLicenseeHooks::NewL();
    OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_DUP4, "CUsbBatteryChargingPlugin::ConstructL;Created licensee specific hooks" );
    
    // Set initial recovery state to idle
    PushRecoverState(EPluginStateIdle);
    
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_CONSTRUCTL_EXIT );
    }

// For host OTG enabled charging plug-in
TBool CUsbBatteryChargingPlugin::IsUsbChargingPossible()
    {
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
    // VBus is off, so there is no way to do charging
    if (0 == iVBusState)
        {
        return EFalse;
        }
    
    // 'A' end cable connected. I'm the power supplier, with no way to charge myself :-) 
    if (EUsbBatteryChargingIdPinARole == iIdPinState)
        {
        return EFalse;
        }

    // BHost charging is disabled
    if (EUsbOtgStateBHost == iOtgState)
        {
        return EFalse;
        }
        
    return ETrue;
#else
    return ETrue;
#endif
    }
/**
 Set first power to request
*/
void CUsbBatteryChargingPlugin::SetInitialConfigurationL()
    {
    OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;Setting Initial Configuration" );
    if (iCurrentValues.Count() > 0)
        {
        TInt err;
        TInt configDescriptorSize = 0;
        err = iLdd.GetConfigurationDescriptorSize(configDescriptorSize);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP1, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;iLdd.GetConfigurationDescriptorSize(configDescriptorSize) with error=%d", err );
            User::Leave(err);
            }
        HBufC8* configDescriptor = HBufC8::NewLC(configDescriptorSize);
        TPtr8 ptr(configDescriptor->Des());

        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP2, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;Getting Configuration Descriptor (size = %d)", configDescriptorSize );
        err = iLdd.GetConfigurationDescriptor(ptr);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP3, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;iLdd.GetConfigurationDescriptor(ptr) with error=%d", err );
            User::Leave(err);
            }

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
        // Get first power to put in configurator
        OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP4, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;Checking IdPin state:" );
        if(iIdPinState == EUsbBatteryChargingIdPinBRole)
#else
        if (ETrue)
#endif
        	{
            if (iCurrentValues.Count() > 0)
                {
                iCurrentIndexRequested = 0;
                iRequestedCurrentValue = iCurrentValues[iCurrentIndexRequested];
                OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP5, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;IdPin state is 0, current set to: %d", iRequestedCurrentValue );
                }
            else
                {
                OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP6, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;No vailable current found !" );
                }
            }
        else
            {
            iRequestedCurrentValue = 0;
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP7, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;IdPin state is 1, current set to 0" );
            }

        TUint oldCurrentValue = ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] << 1;
        ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] = (iRequestedCurrentValue >> 1);

        OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP8, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;Setting Updated Configuration Descriptor" );
        err = iLdd.SetConfigurationDescriptor(ptr);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETINITIALCONFIGURATIONL_DUP9, "CUsbBatteryChargingPlugin::SetInitialConfigurationL;iLdd.SetConfigurationDescriptor(ptr) with error=%d", err );
            User::Leave(err);
            }

        CleanupStack::PopAndDestroy(configDescriptor); 
        }
    }


TAny* CUsbBatteryChargingPlugin::GetInterface(TUid aUid)
    {
    OstTrace1( TRACE_FLOW, REF_CUSBBATTERYCHARGINGPLUGIN_GETINTERFACE, "CUsbBatteryChargingPlugin::GetInterface;this = [0x%08x]", this );
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_GETINTERFACE_DUP1, "CUsbBatteryChargingPlugin::GetInterface;aUid = 0x%08x", aUid.iUid );
    (void)aUid;

    TAny* ret = NULL;

    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_GETINTERFACE_DUP2, "CUsbBatteryChargingPlugin::GetInterface;ret = [0x%08x]", ret );
    return ret;
    }

void CUsbBatteryChargingPlugin::Panic(TUsbBatteryChargingPanic aPanic)
    {
    OstTrace1( TRACE_FATAL, REF_CUSBBATTERYCHARGINGPLUGIN_PANIC, "CUsbBatteryChargingPlugin::Panic;*** CUsbBatteryChargingPlugin::Panic(%d) ***", aPanic );
    _LIT(KUsbChargingPanic,"USB Charging");
    User::Panic(KUsbChargingPanic, aPanic);
    }

void CUsbBatteryChargingPlugin::UsbServiceStateChange(TInt /*aLastError*/, TUsbServiceState /*aOldState*/, TUsbServiceState /*aNewState*/)
    {
    // not used
    }

void CUsbBatteryChargingPlugin::PushRecoverState(TUsbChargingPluginState aRecoverState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_PUSHRECOVERSTATE_ENTRY );

    if((aRecoverState == EPluginStateIdle)||
       (aRecoverState == EPluginStateIdleNegotiated) ||
       (aRecoverState == EPluginStateBEndedCableNotPresent) ||
       (aRecoverState == EPluginStateNoValidCurrent))
        {
        iPluginStateToRecovery = aRecoverState;
        }
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_PUSHRECOVERSTATE_EXIT );
    }

TUsbChargingPluginState CUsbBatteryChargingPlugin::PopRecoverState()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_POPRECOVERSTATE_ENTRY );
    
    SetState(iPluginStateToRecovery);
    
    iPluginStateToRecovery = EPluginStateIdle;
    
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_POPRECOVERSTATE_EXIT );
    return iPluginStateToRecovery;
    }

TUsbChargingPluginState CUsbBatteryChargingPlugin::SetState(TUsbChargingPluginState aState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_SETSTATE_ENTRY );

    switch (aState)
        {
        case EPluginStateIdle:
            {
            ResetPlugin();
            iCurrentState = iPluginStates[aState];
            }
            break;
        case EPluginStateCurrentNegotiating:
        case EPluginStateCharging:
        case EPluginStateNoValidCurrent:
        case EPluginStateIdleNegotiated:
        case EPluginStateUserDisabled:
        case EPluginStateBEndedCableNotPresent:
            {
            iPluginState = aState;
            iCurrentState = iPluginStates[aState];
            }
            break;
        default:
            // Should never happen ...
            iPluginState = EPluginStateIdle;
            iCurrentState = iPluginStates[EPluginStateIdle];
    
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETSTATE, "CUsbBatteryChargingPlugin::SetState;Invalid new state: aState = %d", aState );
            
            Panic(EUsbBatteryChargingPanicUnexpectedPluginState);
        }
    iPluginState = aState;
    
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETSTATE_DUP1, "CUsbBatteryChargingPlugin::SetState;New state: aState=%d", aState );
    
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_SETSTATE_EXIT );
    return iPluginState;
    }

void CUsbBatteryChargingPlugin::NegotiateChargingCurrent()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATECHARGINGCURRENT_ENTRY );

    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATECHARGINGCURRENT, "CUsbBatteryChargingPlugin::NegotiateChargingCurrent;iDeviceState=%d", iDeviceState );
    TRAPD(result, NegotiateNextCurrentValueL());
    if(result == KErrNone)
        {
        SetState(EPluginStateCurrentNegotiating);
        }
    else
        {
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATECHARGINGCURRENT_DUP1, "CUsbBatteryChargingPlugin::NegotiateChargingCurrent;Negotiation call failed, iVBusState = 1: result = %d", result );
        }
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATECHARGINGCURRENT_EXIT );
    }

void CUsbBatteryChargingPlugin::UsbDeviceStateChange(TInt aLastError,
        TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_USBDEVICESTATECHANGE_ENTRY );
    
    iCurrentState->UsbDeviceStateChange(aLastError, aOldState, aNewState);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_USBDEVICESTATECHANGE_EXIT );
    }

void CUsbBatteryChargingPlugin::HandleRepositoryValueChangedL(const TUid& aRepository, TUint aId, TInt aVal)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_HANDLEREPOSITORYVALUECHANGEDL_ENTRY );

    iCurrentState->HandleRepositoryValueChangedL(aRepository, aId, aVal);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_HANDLEREPOSITORYVALUECHANGEDL_EXIT );
    }

void CUsbBatteryChargingPlugin::DeviceStateTimeout()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_DEVICESTATETIMEOUT_ENTRY );
        
    iCurrentState->DeviceStateTimeout();
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_DEVICESTATETIMEOUT_EXIT );
    }

void CUsbBatteryChargingPlugin::NegotiateNextCurrentValueL()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATENEXTCURRENTVALUEL_ENTRY );

    iDeviceStateTimer->Cancel();
    TUint newCurrent = 0;

    if ((iPluginState == EPluginStateIdle)  && iCurrentValues.Count() > 0)
        {
        // i.e. we haven't requested anything yet, and there are some current values to try
        iCurrentIndexRequested  = 0;
        newCurrent = iCurrentValues[iCurrentIndexRequested];        
        }
    else if (iPluginState == EPluginStateCurrentNegotiating && ( iCurrentIndexRequested + 1) < iCurrentValues.Count())
        {
        // there are more current values left to try
        iCurrentIndexRequested++;
        newCurrent = iCurrentValues[iCurrentIndexRequested];    
        }
    else if(iRequestedCurrentValue != 0)
        {
        // There isn't a 0ma round set from the Repository source -> 10208DD7.txt
        // Just add it to make sure the device can be accepted by host    
        newCurrent = 0;
        }
    else
        {
        // Warning 0001: If you go here, something wrong happend, check it.
        __ASSERT_DEBUG(0,Panic(EUsbBatteryChargingPanicBadCharingCurrentNegotiation));
        }
    
    RequestCurrentL(newCurrent);
    iRequestedCurrentValue = newCurrent;
    iPluginState = EPluginStateCurrentNegotiating;
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_NEGOTIATENEXTCURRENTVALUEL_EXIT );
    }

void CUsbBatteryChargingPlugin::ResetPlugin()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_RESETPLUGIN_ENTRY );
    
    if((iPluginState != EPluginStateIdle))
        {
        iDeviceStateTimer->Cancel(); // doesn't matter if not running
        iPluginState = EPluginStateIdle;
        iPluginStateToRecovery = EPluginStateIdle;
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_RESETPLUGIN, "CUsbBatteryChargingPlugin::ResetPlugin;PluginState => EPluginStateIdle(%d)", iPluginState );

        iRequestedCurrentValue = 0;
        iCurrentIndexRequested = 0;
        iAvailableMilliAmps = 0;
        SetNegotiatedCurrent(0);
        TRAP_IGNORE(SetInitialConfigurationL());
        }
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_RESETPLUGIN_EXIT );
    }

void CUsbBatteryChargingPlugin::RequestCurrentL(TUint aMilliAmps)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_ENTRY );
    
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL, "CUsbBatteryChargingPlugin::RequestCurrentL;aMilliAmps=%u", aMilliAmps );

    if((EPluginStateCurrentNegotiating == iPluginState) && (iRequestedCurrentValue != aMilliAmps))
        {
        TInt err;
        TInt configDescriptorSize = 0;
        err = iLdd.GetConfigurationDescriptorSize(configDescriptorSize);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP1, "CUsbBatteryChargingPlugin::RequestCurrentL;iLdd.GetConfigurationDescriptorSize(configDescriptorSize) with error=%d", err );
            User::Leave(err);
            }
        HBufC8* configDescriptor = HBufC8::NewLC(configDescriptorSize);
        TPtr8 ptr(configDescriptor->Des());

        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP2, "CUsbBatteryChargingPlugin::RequestCurrentL;Getting Configuration Descriptor (size = %d)", configDescriptorSize );
        err = iLdd.GetConfigurationDescriptor(ptr);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP3, "CUsbBatteryChargingPlugin::RequestCurrentL;iLdd.GetConfigurationDescriptor(ptr) with error=%d", err );
            User::Leave(err);
            }

        // set bMaxPower field. One unit = 2mA, so need to halve aMilliAmps.
        OstTraceExt2( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP4, "CUsbBatteryChargingPlugin::RequestCurrentL;Setting bMaxPower to %u mA ( = %u x 2mA units)", aMilliAmps, aMilliAmps >> 1 );
        TUint oldCurrentValue = ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] << 1;
        OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP5, "CUsbBatteryChargingPlugin::RequestCurrentL;(old value was %u mA)", oldCurrentValue );

        //since the device will force reEnumeration if the value is odd
        aMilliAmps = aMilliAmps & 0xFFFE;    
    
        // to negotiate a new current value, ReEnumerate is needed
        OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP6, "CUsbBatteryChargingPlugin::RequestCurrentL;Forcing ReEnumeration" );
        ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] = (aMilliAmps >> 1);
        OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP7, "CUsbBatteryChargingPlugin::RequestCurrentL;Setting Updated Configuration Descriptor" );
        err = iLdd.SetConfigurationDescriptor(ptr);
        if(err < 0)
            {
            OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP8, "CUsbBatteryChargingPlugin::RequestCurrentL;iLdd.SetConfigurationDescriptor(ptr) with err=%d", err );
            User::Leave(err);
            }
        OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP9, "CUsbBatteryChargingPlugin::RequestCurrentL;Triggering Re-enumeration" );
        iDeviceReEnumerator->ReEnumerate();
        
        CleanupStack::PopAndDestroy(configDescriptor); // configDescriptor
        }    
    
    // Always issue a timer as a watchdog to monitor the request progress
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_DUP10, "CUsbBatteryChargingPlugin::RequestCurrentL;Starting timer: %d", User::NTickCount() );
    iDeviceStateTimer->Cancel();
    iDeviceStateTimer->Start(TTimeIntervalMicroSeconds32(KUsbBatteryChargingCurrentRequestTimeout));
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_REQUESTCURRENTL_EXIT );
    }

void CUsbBatteryChargingPlugin::ReadCurrentRequestValuesL()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_READCURRENTREQUESTVALUESL_ENTRY );
    
    CRepository* repository = CRepository::NewLC(KUsbBatteryChargingCentralRepositoryUid);

    TInt numberOfCurrents = 0;
    repository->Get(KUsbBatteryChargingKeyNumberOfCurrentValues, numberOfCurrents);

    TInt i = 0;
    for (i=0; i<numberOfCurrents; i++)
        {
        TInt value;
        repository->Get(KUsbBatteryChargingCurrentValuesOffset + i, value);
        iCurrentValues.Append(static_cast<TUint>(value));
        OstTraceExt2( TRACE_FLOW, REF_CUSBBATTERYCHARGINGPLUGIN_READCURRENTREQUESTVALUESL, "CUsbBatteryChargingPlugin::ReadCurrentRequestValuesL;CurrentValue %d = %dmA", i, value );
        }

    CleanupStack::PopAndDestroy(repository);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_READCURRENTREQUESTVALUESL_EXIT );
    }

void CUsbBatteryChargingPlugin::StartCharging(TUint aMilliAmps)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_STARTCHARGING_ENTRY );
    
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_STARTCHARGING, "CUsbBatteryChargingPlugin::StartCharging;aMilliAmps=%u", aMilliAmps );
    
    // do licensee specific functionality (if any)
    iLicenseeHooks->StartCharging(aMilliAmps);

#ifdef _DEBUG
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingChargingCurrent,
                            aMilliAmps);
    OstTraceExt2( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_STARTCHARGING_DUP1, "CUsbBatteryChargingPlugin::StartCharging;Set P&S current = %umA - err = %d", aMilliAmps, err );
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingChargingCurrent,
                                aMilliAmps);
#endif

    SetState(EPluginStateCharging);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_STARTCHARGING_EXIT );
    }

void CUsbBatteryChargingPlugin::StopCharging()
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_STOPCHARGING_ENTRY );
    
    // do licensee specific functionality (if any)
    iLicenseeHooks->StopCharging();

#ifdef _DEBUG
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingChargingCurrent,
                                    0);
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_STOPCHARGING, "CUsbBatteryChargingPlugin::StopCharging;Set P&S current = 0mA - err = %d", err );
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingChargingCurrent,
                                    0);
#endif
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_STOPCHARGING_EXIT );
    }

void CUsbBatteryChargingPlugin::SetNegotiatedCurrent(TUint aMilliAmps)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_SETNEGOTIATEDCURRENT_ENTRY );
    
    OstTrace1( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETNEGOTIATEDCURRENT, "CUsbBatteryChargingPlugin::SetNegotiatedCurrent;aMilliAmps=%u", aMilliAmps );

    // Ignore errors - not much we can do if it fails
#ifdef _DEBUG
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingAvailableCurrent,
                                    aMilliAmps);
    OstTraceExt2( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_SETNEGOTIATEDCURRENT_DUP1, "CUsbBatteryChargingPlugin::SetNegotiatedCurrent;Set P&S current = %umA - err = %d", aMilliAmps, err );
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingAvailableCurrent,
                                    aMilliAmps);
#endif
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_SETNEGOTIATEDCURRENT_EXIT );
    }


#ifndef _DEBUG
void CUsbBatteryChargingPlugin::LogStateText(TUsbDeviceState /*aState*/)
    {
    }
#else
void CUsbBatteryChargingPlugin::LogStateText(TUsbDeviceState aState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_ENTRY );
    
    switch (aState)
        {
        case EUsbDeviceStateUndefined:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT, "CUsbBatteryChargingPlugin::LogStateText; ***** UNDEFINED *****" );
            break;
        case EUsbDeviceStateDefault:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP1, "CUsbBatteryChargingPlugin::LogStateText; ***** DEFAULT *****" );
            break;
        case EUsbDeviceStateAttached:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP2, "CUsbBatteryChargingPlugin::LogStateText; ***** ATTACHED *****" );
            break;
        case EUsbDeviceStatePowered:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP3, "CUsbBatteryChargingPlugin::LogStateText; ***** POWERED *****" );
            break;
        case EUsbDeviceStateConfigured:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP4, "CUsbBatteryChargingPlugin::LogStateText; ***** CONFIGURED *****" );
            break;
        case EUsbDeviceStateAddress:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP5, "CUsbBatteryChargingPlugin::LogStateText; ***** ADDRESS *****" );
            break;
        case EUsbDeviceStateSuspended:
            OstTrace0( TRACE_NORMAL, REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_DUP6, "CUsbBatteryChargingPlugin::LogStateText; ***** SUSPENDED *****" );
            break;
        default:
            break;
        }
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_LOGSTATETEXT_EXIT );
    }
#endif

void CUsbBatteryChargingPlugin::MpsoVBusStateChanged(TInt aNewState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOVBUSSTATECHANGED_ENTRY );
    
    iCurrentState->MpsoVBusStateChanged(aNewState);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOVBUSSTATECHANGED_EXIT );
    }


// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void CUsbBatteryChargingPlugin::MpsoIdPinStateChanged(TInt aValue)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOIDPINSTATECHANGED_ENTRY );
    
    iCurrentState->MpsoIdPinStateChanged(aValue);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOIDPINSTATECHANGED_EXIT );
    }

void CUsbBatteryChargingPlugin::MpsoOtgStateChangedL(TUsbOtgState aNewState)
    {
    OstTraceFunctionEntry0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOOTGSTATECHANGEDL_ENTRY );

    iCurrentState->MpsoOtgStateChangedL(aNewState);
    OstTraceFunctionExit0( REF_CUSBBATTERYCHARGINGPLUGIN_MPSOOTGSTATECHANGEDL_EXIT );
    }
#endif
