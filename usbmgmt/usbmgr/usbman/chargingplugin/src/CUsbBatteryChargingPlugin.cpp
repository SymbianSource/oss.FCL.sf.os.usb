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

#include "CUsbBatteryChargingPlugin.h"
#include "chargingstates.h"
#include <musbmanextensionpluginobserver.h>
#include "cusbbatterycharginglicenseehooks.h"
#include "reenumerator.h"
#include <usb/usblogger.h>
#include <e32property.h>
#include <centralrepository.h>
#include <usbotgdefs.h>

#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV          // For host OTG enabled charging plug-in 
#include "idpinwatcher.h"
#include "otgstatewatcher.h"
#endif

#include "vbuswatcher.h"
#include <e32debug.h> 
#include <e32def.h>

static const TInt KUsbBatteryChargingConfigurationDescriptorCurrentOffset = 8; // see bMaxPower in section 9.6.3 of USB Spec 2.0
static const TInt KUsbBatteryChargingCurrentRequestTimeout = 3000000; // 3 seconds

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBCHARGE");
#endif

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
    LOGTEXT(KNullDesC8);
    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::~CUsbBatteryChargingPlugin this = [0x%08x]"), this);
    
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
    LOGTEXT(_L8(">>CUsbBatteryChargingPlugin::ConstructL"));
   
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
        LEAVEL(err);
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
        LEAVEL(err);
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
        LEAVEL(err);
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
        LOGTEXT2(_L8("PluginState => EPluginStateADevice(%d)"), iPluginState);
        }

    Observer().RegisterStateObserverL(*this);

    iLicenseeHooks = CUsbBatteryChargingLicenseeHooks::NewL();
    LOGTEXT(_L8("Created licensee specific hooks"));
    
    // Set initial recovery state to idle
    PushRecoverState(EPluginStateIdle);
    
    LOGTEXT(_L8("<<CUsbBatteryChargingPlugin::ConstructL"));
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
    LOGTEXT(_L8("Setting Initial Configuration"));
    if (iCurrentValues.Count() > 0)
        {
        TInt configDescriptorSize = 0;
        LEAVEIFERRORL(iLdd.GetConfigurationDescriptorSize(configDescriptorSize));
        HBufC8* configDescriptor = HBufC8::NewLC(configDescriptorSize);
        TPtr8 ptr(configDescriptor->Des());

        LOGTEXT2(_L8("Getting Configuration Descriptor (size = %d)"),configDescriptorSize);
        LEAVEIFERRORL(iLdd.GetConfigurationDescriptor(ptr));

// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
        // Get first power to put in configurator
        LOGTEXT(_L8("Checking IdPin state:"));
        if(iIdPinState == EUsbBatteryChargingIdPinBRole)
#else
        if (ETrue)
#endif
        	{
            if (iCurrentValues.Count() > 0)
                {
                iCurrentIndexRequested = 0;
                iRequestedCurrentValue = iCurrentValues[iCurrentIndexRequested];
                LOGTEXT2(_L8("IdPin state is 0, current set to: %d"), iRequestedCurrentValue);
                }
            else
                {
                LOGTEXT(_L8("No vailable current found !"));
                }
            }
        else
            {
            iRequestedCurrentValue = 0;
            LOGTEXT(_L8("IdPin state is 1, current set to 0"));
            }

        TUint oldCurrentValue = ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] << 1;
        ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] = (iRequestedCurrentValue >> 1);

        LOGTEXT(_L8("Setting Updated Configuration Descriptor"));
        LEAVEIFERRORL(iLdd.SetConfigurationDescriptor(ptr));

        CleanupStack::PopAndDestroy(configDescriptor); 
        }
    }


TAny* CUsbBatteryChargingPlugin::GetInterface(TUid aUid)
    {
    LOGTEXT(KNullDesC8);
    LOGTEXT3(_L8(">>CUsbBatteryChargingPlugin::GetInterface this = [0x%08x], aUid = 0x%08x"), this, aUid);
    (void)aUid;

    TAny* ret = NULL;

    LOGTEXT2(_L8("<<CUsbBatteryChargingPlugin::GetInterface ret = [0x%08x]"), ret);
    return ret;
    }

void CUsbBatteryChargingPlugin::Panic(TUsbBatteryChargingPanic aPanic)
    {
    LOGTEXT2(_L8("*** CUsbBatteryChargingPlugin::Panic(%d) ***"),aPanic);
    _LIT(KUsbChargingPanic,"USB Charging");
    User::Panic(KUsbChargingPanic, aPanic);
    }

void CUsbBatteryChargingPlugin::UsbServiceStateChange(TInt /*aLastError*/, TUsbServiceState /*aOldState*/, TUsbServiceState /*aNewState*/)
    {
    // not used
    }

void CUsbBatteryChargingPlugin::PushRecoverState(TUsbChargingPluginState aRecoverState)
    {
    LOG_FUNC

    if((aRecoverState == EPluginStateIdle)||
       (aRecoverState == EPluginStateIdleNegotiated) ||
       (aRecoverState == EPluginStateBEndedCableNotPresent) ||
       (aRecoverState == EPluginStateNoValidCurrent))
        {
        iPluginStateToRecovery = aRecoverState;
        }
    }

TUsbChargingPluginState CUsbBatteryChargingPlugin::PopRecoverState()
    {
    LOG_FUNC
    
    SetState(iPluginStateToRecovery);
    
    iPluginStateToRecovery = EPluginStateIdle;
    
    return iPluginStateToRecovery;
    }

TUsbChargingPluginState CUsbBatteryChargingPlugin::SetState(TUsbChargingPluginState aState)
    {
    LOG_FUNC

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
    
            LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::SetState: Invalid new state: aState = %d"), aState);
            
            Panic(EUsbBatteryChargingPanicUnexpectedPluginState);
        }
    iPluginState = aState;
    
    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::SetState, New state: aState = %d"), aState);
    
    return iPluginState;
    }

void CUsbBatteryChargingPlugin::NegotiateChargingCurrent()
    {
    LOG_FUNC

    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::StartNegotiation,  iDeviceState = %d"), iDeviceState);
    TRAPD(result, NegotiateNextCurrentValueL());
    if(result == KErrNone)
        {
        SetState(EPluginStateCurrentNegotiating);
        }
    else
        {
        LOGTEXT2(_L8("Negotiation call failed, iVBusState = 1: result = %d"), result);
        }
    }

void CUsbBatteryChargingPlugin::UsbDeviceStateChange(TInt aLastError,
        TUsbDeviceState aOldState, TUsbDeviceState aNewState)
    {
    LOG_FUNC
    
    iCurrentState->UsbDeviceStateChange(aLastError, aOldState, aNewState);
    }

void CUsbBatteryChargingPlugin::HandleRepositoryValueChangedL(const TUid& aRepository, TUint aId, TInt aVal)
    {
    LOG_FUNC    

    iCurrentState->HandleRepositoryValueChangedL(aRepository, aId, aVal);
    }

void CUsbBatteryChargingPlugin::DeviceStateTimeout()
    {
    LOG_FUNC
        
    iCurrentState->DeviceStateTimeout();
    }

void CUsbBatteryChargingPlugin::NegotiateNextCurrentValueL()
    {
    LOG_FUNC

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
    }

void CUsbBatteryChargingPlugin::ResetPlugin()
    {
    LOG_FUNC
    
    if((iPluginState != EPluginStateIdle))
        {
        iDeviceStateTimer->Cancel(); // doesn't matter if not running
        iPluginState = EPluginStateIdle;
        iPluginStateToRecovery = EPluginStateIdle;
        LOGTEXT2(_L8("PluginState => EPluginStateIdle(%d)"),iPluginState);

        iRequestedCurrentValue = 0;
        iCurrentIndexRequested = 0;
        iAvailableMilliAmps = 0;
        SetNegotiatedCurrent(0);
        TRAP_IGNORE(SetInitialConfigurationL());
        }
    }

void CUsbBatteryChargingPlugin::RequestCurrentL(TUint aMilliAmps)
    {
    LOG_FUNC
    
    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::RequestCurrent aMilliAmps = %d"), aMilliAmps);

    if((EPluginStateCurrentNegotiating == iPluginState) && (iRequestedCurrentValue != aMilliAmps))
        {
        TInt configDescriptorSize = 0;
        LEAVEIFERRORL(iLdd.GetConfigurationDescriptorSize(configDescriptorSize));
        HBufC8* configDescriptor = HBufC8::NewLC(configDescriptorSize);
        TPtr8 ptr(configDescriptor->Des());

        LOGTEXT2(_L8("Getting Configuration Descriptor (size = %d)"),configDescriptorSize);
        LEAVEIFERRORL(iLdd.GetConfigurationDescriptor(ptr));

        // set bMaxPower field. One unit = 2mA, so need to halve aMilliAmps.
        LOGTEXT3(_L8("Setting bMaxPower to %d mA ( = %d x 2mA units)"),aMilliAmps, (aMilliAmps >> 1));
        TUint oldCurrentValue = ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] << 1;
        LOGTEXT2(_L8("(old value was %d mA)"), oldCurrentValue);

        //since the device will force reEnumeration if the value is odd
        aMilliAmps = aMilliAmps & 0xFFFE;    
    
        // to negotiate a new current value, ReEnumerate is needed
        LOGTEXT(_L8("Forcing ReEnumeration"));
        ptr[KUsbBatteryChargingConfigurationDescriptorCurrentOffset] = (aMilliAmps >> 1);
        LOGTEXT(_L8("Setting Updated Configuration Descriptor"));
        LEAVEIFERRORL(iLdd.SetConfigurationDescriptor(ptr));
        LOGTEXT(_L8("Triggering Re-enumeration"));
        iDeviceReEnumerator->ReEnumerate();
        
        CleanupStack::PopAndDestroy(configDescriptor); // configDescriptor
        }    
    
    // Always issue a timer as a watchdog to monitor the request progress
    LOGTEXT2(_L8("Starting timer: %d"), User::NTickCount());
    iDeviceStateTimer->Cancel();
    iDeviceStateTimer->Start(TTimeIntervalMicroSeconds32(KUsbBatteryChargingCurrentRequestTimeout));
    }

void CUsbBatteryChargingPlugin::ReadCurrentRequestValuesL()
    {
    LOG_FUNC
    
    CRepository* repository = CRepository::NewLC(KUsbBatteryChargingCentralRepositoryUid);

    TInt numberOfCurrents = 0;
    repository->Get(KUsbBatteryChargingKeyNumberOfCurrentValues, numberOfCurrents);

    TInt i = 0;
    for (i=0; i<numberOfCurrents; i++)
        {
        TInt value;
        repository->Get(KUsbBatteryChargingCurrentValuesOffset + i, value);
        iCurrentValues.Append(static_cast<TUint>(value));
        LOGTEXT3(_L8("CurrentValue %d = %dmA"),i,value);
        }

    CleanupStack::PopAndDestroy(repository);
    }

void CUsbBatteryChargingPlugin::StartCharging(TUint aMilliAmps)
    {
    LOG_FUNC
    
    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::StartCharging aMilliAmps = %d"), aMilliAmps);
    
    // do licensee specific functionality (if any)
    iLicenseeHooks->StartCharging(aMilliAmps);

#ifdef __FLOG_ACTIVE
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingChargingCurrent,
                            aMilliAmps);
    LOGTEXT3(_L8("Set P&S current = %dmA - err = %d"),aMilliAmps,err);
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                KPropertyUidUsbBatteryChargingChargingCurrent,
                                aMilliAmps);
#endif

    SetState(EPluginStateCharging);
    }

void CUsbBatteryChargingPlugin::StopCharging()
    {
    LOG_FUNC
    
    // do licensee specific functionality (if any)
    iLicenseeHooks->StopCharging();

#ifdef __FLOG_ACTIVE
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingChargingCurrent,
                                    0);
    LOGTEXT2(_L8("Set P&S current = 0mA - err = %d"),err);
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingChargingCurrent,
                                    0);
#endif
    }

void CUsbBatteryChargingPlugin::SetNegotiatedCurrent(TUint aMilliAmps)
    {
    LOG_FUNC
    
    LOGTEXT2(_L8(">>CUsbBatteryChargingPlugin::SetNegotiatedCurrent aMilliAmps = %d"), aMilliAmps);

    // Ignore errors - not much we can do if it fails
#ifdef __FLOG_ACTIVE
    TInt err = RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingAvailableCurrent,
                                    aMilliAmps);
    LOGTEXT3(_L8("Set P&S current = %dmA - err = %d"),aMilliAmps,err);
#else
    (void)RProperty::Set(KPropertyUidUsbBatteryChargingCategory,
                                    KPropertyUidUsbBatteryChargingAvailableCurrent,
                                    aMilliAmps);
#endif
    }


#ifndef __FLOG_ACTIVE
void CUsbBatteryChargingPlugin::LogStateText(TUsbDeviceState /*aState*/)
    {
    LOG_FUNC
    }
#else
void CUsbBatteryChargingPlugin::LogStateText(TUsbDeviceState aState)
    {
    LOG_FUNC
    
    switch (aState)
        {
        case EUsbDeviceStateUndefined:
            LOGTEXT(_L8(" ***** UNDEFINED *****"));
            break;
        case EUsbDeviceStateDefault:
            LOGTEXT(_L8(" ***** DEFAULT *****"));
            break;
        case EUsbDeviceStateAttached:
            LOGTEXT(_L8(" ***** ATTACHED *****"));
            break;
        case EUsbDeviceStatePowered:
            LOGTEXT(_L8(" ***** POWERED *****"));
            break;
        case EUsbDeviceStateConfigured:
            LOGTEXT(_L8(" ***** CONFIGURED *****"));
            break;
        case EUsbDeviceStateAddress:
            LOGTEXT(_L8(" ***** ADDRESS *****"));
            break;
        case EUsbDeviceStateSuspended:
            LOGTEXT(_L8(" ***** SUSPENDED *****"));
            break;
        default:
            break;
        }
    }
#endif

void CUsbBatteryChargingPlugin::MpsoVBusStateChanged(TInt aNewState)
    {
    LOG_FUNC
    
    iCurrentState->MpsoVBusStateChanged(aNewState);
    }


// For host OTG enabled charging plug-in
#ifdef SYMBIAN_ENABLE_USB_OTG_HOST_PRIV
void CUsbBatteryChargingPlugin::MpsoIdPinStateChanged(TInt aValue)
    {
    LOG_FUNC
    
    iCurrentState->MpsoIdPinStateChanged(aValue);
    }

void CUsbBatteryChargingPlugin::MpsoOtgStateChangedL(TUsbOtgState aNewState)
    {
    LOG_FUNC

    iCurrentState->MpsoOtgStateChangedL(aNewState);
    }
#endif
