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
* Implements the object of Usbman that manages all the USB OTG
* activity (via CUsbOtgWatcher).
*
*/

/**
 @file
*/

#include "CUsbOtg.h"
#include "cusbotgwatcher.h"
#include "CUsbDevice.h"
#include "musbotghostnotifyobserver.h"
#include "CUsbServer.h"
#include <e32property.h> //Publish & Subscribe header
#include "usberrors.h"

//Name used in call to User::LoadLogicalDevice/User::FreeLogicalDevice
_LIT(KUsbOtgLDDName,"otgdi");

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR-OTG");
#endif


CUsbOtg* CUsbOtg::NewL()
/**
 * Constructs a CUsbOtg object.
 *
 * @return	A new CUsbOtg object
 */
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtg* self = new (ELeave) CUsbOtg();
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


CUsbOtg::~CUsbOtg()
/**
 * Destructor.
 */
	{
	LOG_FUNC

	// Cancel any outstanding asynchronous operation.
	Stop();
	
	// Free any memory allocated to the list of observers. Note that
	// we don't want to call ResetAndDestroy, because we don't own
	// the observers themselves.
	iObservers.Reset();
	
	LOGTEXT2(_L8("about to stop Id-Pin watcher @ %08x"), (TUint32) iIdPinWatcher);
	if (iIdPinWatcher)
		{
		iIdPinWatcher->Cancel();
		delete iIdPinWatcher;
		iIdPinWatcher = NULL;
		LOGTEXT(_L8("deleted Id-Pin watcher"));
		}
	LOGTEXT2(_L8("about to stop Vbus watcher @ %08x"), (TUint32) iVbusWatcher);
	if (iVbusWatcher)
		{
		iVbusWatcher->Cancel();
		delete iVbusWatcher;
		iVbusWatcher = NULL;
		LOGTEXT(_L8("deleted Vbus watcher"));
		}
	LOGTEXT2(_L8("about to stop OTG State watcher @ %08x"), (TUint32) iVbusWatcher);
	if (iOtgStateWatcher)
		{
		iOtgStateWatcher->Cancel();
		delete iOtgStateWatcher;
		iOtgStateWatcher = NULL;
		LOGTEXT(_L8("deleted OTG State watcher"));
		}
	LOGTEXT2(_L8("about to stop OTG Event watcher @ %08x"), (TUint32) iVbusWatcher);
	if (iOtgEventWatcher)
		{
		iOtgEventWatcher->Cancel();
		delete iOtgEventWatcher;
		iOtgEventWatcher = NULL;
		LOGTEXT(_L8("deleted OTG Event watcher"));
		}
	
	if (iRequestSessionWatcher)
		{
		delete iRequestSessionWatcher;
		LOGTEXT(_L8("deleted Session Request watcher"));
		}

	LOGTEXT2(_L8("about to stop Connection Idle watcher @ %08x"), (TUint32)iOtgConnectionIdleWatcher);
	if (iOtgConnectionIdleWatcher)
		{
		iOtgConnectionIdleWatcher->Cancel();
		delete iOtgConnectionIdleWatcher;
		iOtgConnectionIdleWatcher= NULL;
		LOGTEXT(_L8("deleted Connection Idle watcher"));
		}

	// Unload OTGDI components if it was ever started
	if ( iOtgDriver.Handle() )
		{
		LOGTEXT(_L8("Stopping stacks"));
		iOtgDriver.StopStacks();
		iOtgDriver.Close();
		}
	else
		{
		LOGTEXT(_L8("No OTG Driver session was opened, nothing to do"));
		}

	LOGTEXT(_L8("Freeing logical device"));
	TInt err = User::FreeLogicalDevice(KUsbOtgLDDName);
	//Putting the LOGTEXT2 inside the if statement prevents a compiler
	//warning about err being unused in UREL builds.
	if(err)
		{
		LOGTEXT2(_L8("     User::FreeLogicalDevice returned %d"),err);
		}
	
	iCriticalSection.Close();
	}


CUsbOtg::CUsbOtg()
/**
 * Constructor.
 */
	{
	LOG_FUNC
	}


void CUsbOtg::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC
	
	LOGTEXT(_L8("About to open LDD"));
	iLastError = User::LoadLogicalDevice(KUsbOtgLDDName);
	if ( (iLastError != KErrNone) && (iLastError != KErrAlreadyExists) )
		{
		LOGTEXT3(_L8("Error %d: Unable to load driver: %S"), iLastError, &KUsbOtgLDDName);
		LEAVEIFERRORL(iLastError);
		}

	LOGTEXT(_L8("About to open RUsbOtgDriver"));
	iLastError = iOtgDriver.Open();
	if ( (iLastError != KErrNone) && (iLastError != KErrAlreadyExists) )
		{
		LOGTEXT2(_L8("Error %d: Unable to open RUsbOtgDriver session"), iLastError);
		LEAVEIFERRORL(iLastError);
		}


	LOGTEXT(_L8("About to start OTG stacks"));
	iLastError = iOtgDriver.StartStacks();
	if (iLastError != KErrNone)
		{
		LOGTEXT2(_L8("Error %d: Unable to open start OTG stacks"), iLastError);
		LEAVEIFERRORL(iLastError);
		}

	// Request Otg notifications
	iIdPinWatcher = CUsbOtgIdPinWatcher::NewL(iOtgDriver);
	iIdPinWatcher->Start();

	iVbusWatcher = CUsbOtgVbusWatcher::NewL(iOtgDriver);
	iVbusWatcher->Start();

	iOtgStateWatcher = CUsbOtgStateWatcher::NewL(iOtgDriver);
	iOtgStateWatcher->Start();
	
	iOtgEventWatcher = CUsbOtgEventWatcher::NewL(*this, iOtgDriver, iOtgEvent);
	iOtgEventWatcher->Start();
	
    iOtgConnectionIdleWatcher = CUsbOtgConnectionIdleWatcher::NewL(iOtgDriver);
    iOtgConnectionIdleWatcher->Start();

	iRequestSessionWatcher = CRequestSessionWatcher::NewL(*this);
	
	iCriticalSection.CreateLocal(EOwnerProcess);
	
	LOGTEXT(_L8("UsbOtg::ConstructL() finished"));
	}
	
void CUsbOtg::NotifyMessage(TInt aMessage)
/**
 * The CUsbOtg::NotifyMessage method
 *
 * Reports the OTG message to the observers
 *
 * @internalComponent
 */
	{
	iCriticalSection.Wait();

	TInt msg = aMessage == 0 ? iOtgMessage : aMessage;
	TUint length = iObservers.Count();
	for (TUint i = 0; i < length; i++)
		{
		iObservers[i]->UsbOtgHostMessage(msg);
		}

	iCriticalSection.Signal();
	}

TInt CUsbOtg::TranslateOtgEvent()
/**
 * The CUsbOtg::TranslateOtgEvent method
 *
 * Attempts to translate the OTG event into OTG message
 *
 * @internalComponent
 */
	{
	TInt otgEvent = KErrBadName;
	switch (iOtgEvent)
	{
	case RUsbOtgDriver::EEventHnpDisabled:
		otgEvent = KUsbMessageHnpDisabled;
		break;
	case RUsbOtgDriver::EEventHnpEnabled:
		otgEvent = KUsbMessageHnpEnabled;
		break;
	case RUsbOtgDriver::EEventSrpReceived:
		otgEvent = KUsbMessageSrpReceived;
		break;
	case RUsbOtgDriver::EEventSrpInitiated:
		otgEvent = KUsbMessageSrpInitiated;
		break;
	case RUsbOtgDriver::EEventVbusRaised:
		otgEvent = KUsbMessageVbusRaised;
		break;
	case RUsbOtgDriver::EEventVbusDropped:
		otgEvent = KUsbMessageVbusDropped;
		break;
	}

	return otgEvent;
	}

void CUsbOtg::NotifyOtgEvent()
/**
 * The CUsbOtg::NotifyOtgEvent method
 *
 * Reports the OTG message translated from OTG Event to the observers
 *
 * @internalComponent
 */
	{
	TUint length = iObservers.Count();
	TInt otgEvent = TranslateOtgEvent();
	if ( otgEvent == KErrBadName )
		{
		LOGTEXT2(_L8("CUsbOtg::NotifyOtgEvent(): OTG event %d was reported, but not propagated"), (TInt) iOtgEvent);
		return;
		}

	for (TUint i = 0; i < length; i++)
		{
		iObservers[i]->UsbOtgHostMessage(otgEvent);
		}
	}

void CUsbOtg::RegisterObserverL(MUsbOtgHostNotifyObserver& aObserver)
/**
 * Register an observer of the OTG events.
 * Presently, the device supports watching state.
 *
 * @param	aObserver	New Observer of the OTG events
 */
	{
	LOG_FUNC

	LEAVEIFERRORL(iObservers.Append(&aObserver));
	}


void CUsbOtg::DeRegisterObserver(MUsbOtgHostNotifyObserver& aObserver)
/**
 * De-registers an existing OTG events observer.
 *
 * @param	aObserver	The existing OTG events observer to be de-registered
 */
	{
	LOG_FUNC

	TInt index = iObservers.Find(&aObserver);

	if (index >= 0 && index < iObservers.Count())
		{
		iObservers.Remove(index);
		}
	}


void CUsbOtg::StartL()
/**
 * Start the USB OTG events watcher
 * Reports errors and OTG events via observer interface.
 */
	{
	LOG_FUNC

	iOtgWatcher = CUsbOtgWatcher::NewL(*this, iOtgDriver, iOtgMessage);
	iOtgWatcher->Start();
	}

void CUsbOtg::Stop()
/**
 * Stop the USB OTG events watcher
 */
	{
	LOG_FUNC

	LOGTEXT2(_L8("about to stop OTG watcher @ %08x"), (TUint32) iOtgWatcher);
	
	if (iOtgWatcher)
		{
		iOtgWatcher->Cancel();
		delete iOtgWatcher;
		iOtgWatcher = NULL;
		LOGTEXT(_L8("deleted OTG watcher"));
		}
	
	iLastError = KErrNone;
	}

TInt CUsbOtg::BusRequest()
	{
	LOG_FUNC
	return iOtgDriver.BusRequest();
	}
	
TInt CUsbOtg::BusRespondSrp()
	{
	LOG_FUNC
	return iOtgDriver.BusRespondSrp();
	}

TInt CUsbOtg::BusClearError()
	{
	LOG_FUNC
	return iOtgDriver.BusClearError();
	}

TInt CUsbOtg::BusDrop()
	{
	LOG_FUNC
	return iOtgDriver.BusDrop();
	}
