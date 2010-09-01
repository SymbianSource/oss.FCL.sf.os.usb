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
* Talks directly to the USB Logical Device Driver (LDD) and 
* watches any state changes
*
*/

/**
 @file
*/

#include <usb/usblogger.h>
#include "CUsbScheduler.h"
#include "cusbotgwatcher.h"
#include "CUsbOtg.h"
#include <usb/usbshared.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "USBSVR-OTGWATCHER");
#endif

static _LIT_SECURITY_POLICY_PASS(KAllowAllPolicy);
static _LIT_SECURITY_POLICY_S1(KNetworkControlPolicy,KUsbmanSvrSid,ECapabilityNetworkControl);
static _LIT_SECURITY_POLICY_C1(KRequestSessionPolicy,ECapabilityCommDD);

//-----------------------------------------------------------------------------
//------------------------------ Helper watchers ------------------------------ 
//-----------------------------------------------------------------------------
//--------------------- Base class for all helper watchers -------------------- 
/**
 * The CUsbOtgBaseWatcher::CUsbOtgBaseWatcher method
 *
 * Constructor
 *
 * @param	aOwner	The device that owns the state watcher
 * @param	aLdd	A reference to the USB Logical Device Driver
 */
CUsbOtgBaseWatcher::CUsbOtgBaseWatcher(RUsbOtgDriver& aLdd)
	: CActive(CActive::EPriorityStandard), iLdd(aLdd)
	{
	LOG_FUNC
	CActiveScheduler::Add(this);
	}

/**
 * The CUsbOtgBaseWatcher::~CUsbOtgBaseWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgBaseWatcher::~CUsbOtgBaseWatcher()
	{
	LOG_FUNC
	Cancel();
	}

/**
 * Instructs the state watcher to start watching.
 */
void CUsbOtgBaseWatcher::Start()
	{
	LOG_FUNC
	Post();
	}

//---------------------------- Id-Pin watcher class --------------------------- 
/**
 * The CUsbOtgIdPinWatcher::NewL method
 *
 * Constructs a new CUsbOtgWatcher object
 *
 * @internalComponent
 * @param	aLdd	A reference to the USB Logical Device Driver
 *
 * @return	A new CUsbOtgWatcher object
 */
CUsbOtgIdPinWatcher* CUsbOtgIdPinWatcher::NewL(RUsbOtgDriver& aLdd)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgIdPinWatcher* self = new (ELeave) CUsbOtgIdPinWatcher(aLdd);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


/**
 * The CUsbOtgIdPinWatcher::~CUsbOtgIdPinWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgIdPinWatcher::~CUsbOtgIdPinWatcher()
	{
	LOG_FUNC
	Cancel();
	RProperty::Delete(KUsbOtgIdPinPresentProperty);
	}

void CUsbOtgIdPinWatcher::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC

	TInt err = RProperty::Define(KUsbOtgIdPinPresentProperty, RProperty::EInt, KAllowAllPolicy, KNetworkControlPolicy);
	if ( err != KErrNone && err != KErrAlreadyExists )
	    {
	    User::LeaveIfError(err);
	    }
	err = RProperty::Set(KUidUsbManCategory,KUsbOtgIdPinPresentProperty,EFalse);
	if ( err != KErrNone )
	    {
	    User::LeaveIfError(err);
	    }
	}

/**
 * The CUsbOtgIdPinWatcher::CUsbOtgIdPinWatcher method
 *
 * Constructor
 *
 * @param	aLdd	A reference to the USB Logical Device Driver
 */

CUsbOtgIdPinWatcher::CUsbOtgIdPinWatcher(RUsbOtgDriver& aLdd)
	: CUsbOtgBaseWatcher(aLdd)
	{
	LOG_FUNC
	}

/**
 * Called when the ID-Pin status change is reported
 */
void CUsbOtgIdPinWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());
	
	Post();

	LOGTEXT(_L8("<<CUsbOtgIdPinWatcher::RunL"));
	}


/**
 * Automatically called when the ID-Pin watcher is cancelled.
 */
void CUsbOtgIdPinWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgIdPinNotification();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgIdPinWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgIdPinWatcher::Post() - About to call QueueOtgIdPinNotification"));
	iLdd.QueueOtgIdPinNotification(iOtgIdPin, iStatus);
	switch (iOtgIdPin)
		{
		case RUsbOtgDriver::EIdPinAPlug:
			if (RProperty::Set(KUidUsbManCategory,KUsbOtgIdPinPresentProperty,ETrue) != KErrNone)
				{
				LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::Post [iOtgIdPin=%d] - failed to set the property value"), iOtgIdPin);
				}
			else
				{
				LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::Post [iOtgIdPin=%d] - property is set to 1"), iOtgIdPin);
				}
			break;
		case RUsbOtgDriver::EIdPinBPlug:
		case RUsbOtgDriver::EIdPinUnknown:
			if (RProperty::Set(KUidUsbManCategory,KUsbOtgIdPinPresentProperty,EFalse) != KErrNone)
				{
				LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::Post [iOtgIdPin=%d] - failed to set the property value"), iOtgIdPin);
				}
			else
				{
				LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::Post [iOtgIdPin=%d] - property is set to 0"), iOtgIdPin);
				}
			break;
		default:
			LOGTEXT2(_L8(">>CUsbOtgIdPinWatcher::Post [iOtgIdPin=%d] is unrecognized, re-request QueueOtgIdPinNotification"), iOtgIdPin);
			break;
		}
	SetActive();
	}

//----------------------------- VBus watcher class ---------------------------- 
/**
 * The CUsbOtgVbusWatcher::NewL method
 *
 * Constructs a new CUsbOtgVbusWatcher object
 *
 * @internalComponent
 * @param	aLdd	A reference to the USB OTG Logical Device Driver
 *
 * @return	A new CUsbOtgVbusWatcher object
 */
CUsbOtgVbusWatcher* CUsbOtgVbusWatcher::NewL(RUsbOtgDriver& aLdd)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgVbusWatcher* self = new (ELeave) CUsbOtgVbusWatcher(aLdd);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


/**
 * The CUsbOtgVbusWatcher::~CUsbOtgVbusWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgVbusWatcher::~CUsbOtgVbusWatcher()
	{
	LOG_FUNC
	Cancel();

	RProperty::Delete(KUsbOtgVBusPoweredProperty);
	}

void CUsbOtgVbusWatcher::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC

	TInt err = RProperty::Define(KUsbOtgVBusPoweredProperty, RProperty::EInt, KAllowAllPolicy, KNetworkControlPolicy);
	if ( err != KErrNone && err != KErrAlreadyExists )
	    {
	    User::LeaveIfError(err);
	    }
	err = RProperty::Set(KUidUsbManCategory,KUsbOtgVBusPoweredProperty,EFalse);
	if ( err != KErrNone )
	    {
	    User::LeaveIfError(err);
	    }
	}

/**
 * The CUsbOtgVbusWatcher::CUsbOtgVbusWatcher method
 *
 * Constructor
 *
 * @param	aLdd	A reference to the USB OTG Logical Device Driver
 */
CUsbOtgVbusWatcher::CUsbOtgVbusWatcher(RUsbOtgDriver& aLdd)
	: CUsbOtgBaseWatcher(aLdd)
	{
	LOG_FUNC
	}

/**
 * Called when the Vbus status is changed
 */
void CUsbOtgVbusWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());

	Post();

	LOGTEXT(_L8("<<CUsbOtgVbusWatcher::RunL"));
	}


/**
 * Automatically called when the VBus status watcher is cancelled.
 */
void CUsbOtgVbusWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgVbusNotification();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgVbusWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgVbusWatcher::Post() - About to call QueueOtgVbusNotification"));
	iLdd.QueueOtgVbusNotification(iOtgVbus, iStatus);
	switch (iOtgVbus)
		{
		case RUsbOtgDriver::EVbusHigh:
			if (RProperty::Set(KUidUsbManCategory,KUsbOtgVBusPoweredProperty,ETrue) != KErrNone)
				{
				LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::Post [iOtgVbus=%d](EVbusHigh) - failed to set the property value"), iOtgVbus);
				}
			else
				{
				LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::Post [iOtgVbus=%d](EVbusHigh) - property is set to ETrue"), iOtgVbus);
				}
			break;
		case RUsbOtgDriver::EVbusLow:
		case RUsbOtgDriver::EVbusUnknown:
			if (RProperty::Set(KUidUsbManCategory,KUsbOtgVBusPoweredProperty,EFalse) != KErrNone)
				{
				LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::Post [iOtgVbus=%d](1 - EVbusLow, 2 - EVbusUnknown) - failed to set the property value"), iOtgVbus);
				}
			else
				{
				LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::Post [iOtgVbus=%d](1 - EVbusLow, 2 - EVbusUnknown) - property is set to EFalse"), iOtgVbus);
				}
			break;
		default:
			LOGTEXT2(_L8(">>CUsbOtgVbusWatcher::RunL [iOtgVbus=%d] is unrecognized, re-request QueueOtgVbusNotification"), iOtgVbus);
			break;
		}
	SetActive();
	}


//-------------------------- OTG State watcher class -------------------------- 
/**
 * The CUsbOtgStateWatcher::NewL method
 *
 * Constructs a new CUsbOtgWatcher object
 *
 * @internalComponent
 * @param	aLdd	A reference to the USB Logical Device Driver
 *
 * @return	A new CUsbOtgWatcher object
 */
CUsbOtgStateWatcher* CUsbOtgStateWatcher::NewL(RUsbOtgDriver& aLdd)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgStateWatcher* self = new (ELeave) CUsbOtgStateWatcher(aLdd);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


/**
 * The CUsbOtgStateWatcher::~CUsbOtgStateWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgStateWatcher::~CUsbOtgStateWatcher()
	{
	LOG_FUNC
	Cancel();
	RProperty::Delete(KUsbOtgStateProperty);
	}

void CUsbOtgStateWatcher::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC

	TInt err = RProperty::Define(KUsbOtgStateProperty, RProperty::EInt, KAllowAllPolicy, KNetworkControlPolicy);
	if ( err != KErrNone && err != KErrAlreadyExists )
	    {
	    User::LeaveIfError(err);
	    }
	err = RProperty::Set(KUidUsbManCategory,KUsbOtgStateProperty,RUsbOtgDriver::EStateReset);
	if ( err != KErrNone )
	    {
	    User::LeaveIfError(err);
	    }
	}

/**
 * The CUsbOtgIdPinWatcher::CUsbOtgIdPinWatcher method
 *
 * Constructor
 *
 * @param	aLdd	A reference to the USB Logical Device Driver
 */

CUsbOtgStateWatcher::CUsbOtgStateWatcher(RUsbOtgDriver& aLdd)
	: CUsbOtgBaseWatcher(aLdd)
	{
	LOG_FUNC
	iOtgState = RUsbOtgDriver::EStateReset;
	}

/**
 * Called when the OTG State change is reported
 */
void CUsbOtgStateWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CUsbOtgStateWatcher::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());

	Post();

	LOGTEXT(_L8("<<CUsbOtgStateWatcher::RunL"));
	}


/**
 * Automatically called when the OTG State watcher is cancelled.
 */
void CUsbOtgStateWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgStateNotification();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgStateWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgStateWatcher::Post() - About to call QueueOtgStateNotification"));	
	iLdd.QueueOtgStateNotification(iOtgState, iStatus);
	LOGTEXT3(_L8(">>CUsbOtgStateWatcher::RunL [iStatus=%d], iOtgState = %d"), iStatus.Int(), iOtgState);
	if (RProperty::Set(KUidUsbManCategory,KUsbOtgStateProperty,(TInt)iOtgState) != KErrNone)
	{
		LOGTEXT3(_L8(">>CUsbOtgStateWatcher::RunL [iStatus=%d], iOtgState = %d - failed to set the property"), iStatus.Int(), iOtgState);
	}

	SetActive();
	}

//-------------------------- OTG Events watcher class ------------------------- 
/**
 * The CUsbOtgEventWatcher::NewL method
 *
 * Constructs a new CUsbOtgEventWatcher object
 *
 * @internalComponent
 * @param	aOwner		The CUsbOtg that owns the state watcher
 * @param	aLdd		A reference to the USB Logical Device Driver
 * @param	aOtgEvent	A reference to the OTG Event
 *
 * @return	A new CUsbOtgEventWatcher object
 */
CUsbOtgEventWatcher* CUsbOtgEventWatcher::NewL(CUsbOtg& aOwner, RUsbOtgDriver& aLdd,
											   RUsbOtgDriver::TOtgEvent& aOtgEvent)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgEventWatcher* self = new (ELeave) CUsbOtgEventWatcher(aOwner, aLdd, aOtgEvent);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


/**
 * The CUsbOtgEventWatcher::~CUsbOtgEventWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgEventWatcher::~CUsbOtgEventWatcher()
	{
	LOG_FUNC
	Cancel();
	}

void CUsbOtgEventWatcher::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC
	}

/**
 * The CUsbOtgEventWatcher::CUsbOtgEventWatcher method
 *
 * Constructor
 *
 * @param	aOwner		A reference to the CUsbOtg object that owns the state watcher
 * @param	aLdd		A reference to the USB Logical Device Driver
 * @param	aOtgEvent	A reference to the OTG Event
 */
CUsbOtgEventWatcher::CUsbOtgEventWatcher(CUsbOtg& aOwner, RUsbOtgDriver& aLdd, 
										 RUsbOtgDriver::TOtgEvent& aOtgEvent)
	: CUsbOtgBaseWatcher(aLdd), iOwner(aOwner), iOtgEvent(aOtgEvent)
	{
	LOG_FUNC
	}

/**
 * Called when the OTG Event is reported
 */
void CUsbOtgEventWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CUsbOtgEventWatcher::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());
	LOGTEXT2(_L8("CUsbOtgEventWatcher::RunL() - Otg Event reported: %d"), (TInt)iOtgEvent);
	if (  ( iOtgEvent == RUsbOtgDriver::EEventHnpDisabled )
	    ||( iOtgEvent == RUsbOtgDriver::EEventHnpEnabled )
	    ||( iOtgEvent == RUsbOtgDriver::EEventSrpInitiated )
	    ||( iOtgEvent == RUsbOtgDriver::EEventSrpReceived )
	    ||( iOtgEvent == RUsbOtgDriver::EEventVbusRaised )
	    ||( iOtgEvent == RUsbOtgDriver::EEventVbusDropped )
	   )
		{
		iOwner.NotifyOtgEvent();
		LOGTEXT2(_L8("CUsbOtgEventWatcher::RunL() - The owner is notified about Otg Event = %d"), (TInt)iOtgEvent);
		}
	Post();
	LOGTEXT(_L8("<<CUsbOtgEventWatcher::RunL"));
	}

#ifndef __FLOG_ACTIVE
void CUsbOtgEventWatcher::LogEventText(RUsbOtgDriver::TOtgEvent /*aState*/)
	{
	}
#else
void CUsbOtgEventWatcher::LogEventText(RUsbOtgDriver::TOtgEvent aEvent)
	{
	switch (aEvent)
		{
		case RUsbOtgDriver::EEventAPlugInserted:
			LOGTEXT(_L8(" ***** A-Plug Inserted *****"));
			break;
		case RUsbOtgDriver::EEventAPlugRemoved:
			LOGTEXT(_L8(" ***** A-Plug Removed *****"));
			break;
		case RUsbOtgDriver::EEventVbusRaised:
			LOGTEXT(_L8(" ***** VBus Raised *****"));
			break;
		case RUsbOtgDriver::EEventVbusDropped:
			LOGTEXT(_L8(" ***** VBus Dropped *****"));
			break;
		case RUsbOtgDriver::EEventSrpInitiated:
			LOGTEXT(_L8(" ***** SRP Initiated *****"));
			break;
		case RUsbOtgDriver::EEventSrpReceived:
			LOGTEXT(_L8(" ***** SRP Received *****"));
			break;
		case RUsbOtgDriver::EEventHnpEnabled:
			LOGTEXT(_L8(" ***** HNP Enabled *****"));
			break;
		case RUsbOtgDriver::EEventHnpDisabled:
			LOGTEXT(_L8(" ***** HNP Disabled *****"));
			break;
		case RUsbOtgDriver::EEventRoleChangedToHost:
			LOGTEXT(_L8(" ***** Role Changed to Host *****"));
			break;
		case RUsbOtgDriver::EEventRoleChangedToDevice:
			LOGTEXT(_L8(" ***** Role Changed to Device *****"));
			break;
		case RUsbOtgDriver::EEventRoleChangedToIdle:
			LOGTEXT(_L8(" ***** Role Changed to Idle *****"));
			break;
		default:
			break;
		}
	}
#endif

/**
 * Automatically called when the OTG Event watcher is cancelled.
 */
void CUsbOtgEventWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgEventRequest();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgEventWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgEventWatcher::Post() - About to call QueueOtgEventRequest"));	
	iLdd.QueueOtgEventRequest(iOtgEvent, iStatus);
	SetActive();
	}


//-----------------------------------------------------------------------------
//----------------- OTG watcher class to monitor OTG Messages ----------------- 
//-----------------------------------------------------------------------------
/**
 * The CUsbOtgWatcher::NewL method
 *
 * Constructs a new CUsbOtgWatcher object
 *
 * @internalComponent
 * @param	aOwner	A reference to the object that owns the state watcher
 * @param	aLdd	A reference to the USB Logical Device Driver
 *
 * @return	A new CUsbOtgWatcher object
 */
CUsbOtgWatcher* CUsbOtgWatcher::NewL(MUsbOtgObserver& aOwner, RUsbOtgDriver& aLdd, TUint& aOtgMessage)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgWatcher* r = new (ELeave) CUsbOtgWatcher(aOwner, aLdd, aOtgMessage);
	return r;
	}


/**
 * The CUsbOtgWatcher::~CUsbOtgWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgWatcher::~CUsbOtgWatcher()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">CUsbOtgWatcher::~CUsbOtgWatcher (0x%08x)"), (TUint32) this);
	Cancel();
	}


/**
 * The CUsbOtgWatcher::CUsbOtgWatcher method
 *
 * Constructor
 *
 * @param	aOwner	The device that owns the state watcher
 * @param	aLdd	A reference to the USB Logical Device Driver
 */
CUsbOtgWatcher::CUsbOtgWatcher(MUsbOtgObserver& aOwner, RUsbOtgDriver& aLdd, TUint& aOtgMessage)
	: CActive(CActive::EPriorityStandard), iOwner(aOwner), iLdd(aLdd), iOtgMessage(aOtgMessage)
	{
	LOG_FUNC
	CActiveScheduler::Add(this);
	}

/**
 * Called when the OTG component changes its state.
 */
void CUsbOtgWatcher::RunL()
	{
	LOG_FUNC
	if (iStatus.Int() != KErrNone)
		{
		LOGTEXT2(_L8("CUsbOtgWatcher::RunL() - Error = %d"), iStatus.Int());
		return;
		}

	LOGTEXT2(_L8("CUsbOtgWatcher::RunL() - Otg Message reported: %d"), iOtgMessage);
	iOwner.NotifyMessage();

	Post();
	}


/**
 * Automatically called when the state watcher is cancelled.
 */
void CUsbOtgWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgMessageRequest();
	}


/**
 * Instructs the state watcher to start watching.
 */
void CUsbOtgWatcher::Start()
	{
	LOG_FUNC
	Post();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgWatcher::Post() - About to call QueueOtgMessageRequest"));
	iLdd.QueueOtgMessageRequest((RUsbOtgDriver::TOtgMessage&)iOtgMessage, iStatus);
	SetActive();
	}




//-----------------------------------------------------------------------------
//------ A watcher class to monitor the P&S property for VBus marshalling ----- 
//-----------------------------------------------------------------------------

CRequestSessionWatcher* CRequestSessionWatcher::NewL(MUsbOtgObserver& aOwner)
	{
	CRequestSessionWatcher* self = new(ELeave) CRequestSessionWatcher(aOwner);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CRequestSessionWatcher::~CRequestSessionWatcher()
	{
	Cancel();
	iProp.Close();
	}

void CRequestSessionWatcher::ConstructL()
/**
 * Performs 2nd phase construction of the OTG object.
 */
	{
	LOG_FUNC

	TInt err = RProperty::Define(KUsbRequestSessionProperty, RProperty::EInt, KAllowAllPolicy, KRequestSessionPolicy);
	if ( err != KErrNone && err != KErrAlreadyExists )
	    {
	    User::LeaveIfError(err);
	    }
	err = RProperty::Set(KUidUsbManCategory,KUsbRequestSessionProperty,0);
	if ( err != KErrNone )
	    {
	    User::LeaveIfError(err);
	    }
	User::LeaveIfError(iProp.Attach(KUidUsbManCategory, KUsbRequestSessionProperty));
	iProp.Subscribe(iStatus);
	SetActive();
	}

CRequestSessionWatcher::CRequestSessionWatcher(MUsbOtgObserver& aOwner)
	: CActive(CActive::EPriorityStandard), iOwner(aOwner)
	{
	LOG_FUNC
	CActiveScheduler::Add(this);
	}

/**
 * Called when the OTG Event is reported
 */
void CRequestSessionWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CRequestSessionWatcher::RunL [iStatus=%d]"), iStatus.Int());
	RDebug::Printf(">>CRequestSessionWatcher::RunL [iStatus=%d]", iStatus.Int());
	
	iProp.Subscribe(iStatus);
	SetActive();

	TInt val;
	User::LeaveIfError(iProp.Get(val));
	RDebug::Printf(">>value=%d", val);

	iOwner.NotifyMessage(KUsbMessageRequestSession);
	
	LOGTEXT(_L8("<<CRequestSessionWatcher::RunL"));
	}


/**
 * Automatically called when the OTG Event watcher is cancelled.
 */
void CRequestSessionWatcher::DoCancel()
	{
	LOG_FUNC
	iProp.Cancel();
	}

//---------------------------- Connection Idle watcher class --------------------------- 
/**
 * The CUsbOtgConnectionIdleWatcher::NewL method
 *
 * Constructs a new CUsbOtgWatcher object
 *
 * @internalComponent
 * @param	aLdd	A reference to the USB Logical Device Driver
 *
 * @return	A new CUsbOtgWatcher object
 */
CUsbOtgConnectionIdleWatcher* CUsbOtgConnectionIdleWatcher::NewL(RUsbOtgDriver& aLdd)
	{
	LOG_STATIC_FUNC_ENTRY

	CUsbOtgConnectionIdleWatcher* self = new (ELeave) CUsbOtgConnectionIdleWatcher(aLdd);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}


/**
 * The CUsbOtgConnectionIdleWatcher::~CUsbOtgConnectionIdleWatcher method
 *
 * Destructor
 *
 * @internalComponent
 */
CUsbOtgConnectionIdleWatcher::~CUsbOtgConnectionIdleWatcher()
	{
	LOG_FUNC
	Cancel();
	RProperty::Delete(KUsbOtgConnectionIdleProperty);
	}

/**
 * Performs 2nd phase construction of the OTG object.
 */
void CUsbOtgConnectionIdleWatcher::ConstructL()
	{
	LOG_FUNC

	TInt err = RProperty::Define(KUsbOtgConnectionIdleProperty, RProperty::EInt, KAllowAllPolicy, KNetworkControlPolicy);
	if ( err != KErrNone && err != KErrAlreadyExists )
	    {
	    User::LeaveIfError(err);
	    }
	err = RProperty::Set(KUidUsbManCategory,KUsbOtgConnectionIdleProperty,ETrue);
	if ( err != KErrNone )
	    {
	    User::LeaveIfError(err);
	    }
	}

/**
 * The CUsbOtgConnectionIdleWatcher::CUsbOtgConnectionIdleWatcher method
 *
 * Constructor
 *
 * @param	aLdd	A reference to the USB Logical Device Driver
 */

CUsbOtgConnectionIdleWatcher::CUsbOtgConnectionIdleWatcher(RUsbOtgDriver& aLdd)
	: CUsbOtgBaseWatcher(aLdd)
	{
	LOG_FUNC
	}

/**
 * Called when the Connection Idle status change is reported
 */
void CUsbOtgConnectionIdleWatcher::RunL()
	{
	LOG_FUNC
	LOGTEXT2(_L8(">>CUsbOtgConnectionIdleWatcher::RunL [iStatus=%d]"), iStatus.Int());

	LEAVEIFERRORL(iStatus.Int());
	
	Post();

	LOGTEXT(_L8("<<CUsbOtgConnectionIdleWatcher::RunL"));
	}


/**
 * Automatically called when the Connection Idle watcher is cancelled.
 */
void CUsbOtgConnectionIdleWatcher::DoCancel()
	{
	LOG_FUNC
	iLdd.CancelOtgConnectionNotification();
	}

/**
 * Sets state watcher in active state
 */
void CUsbOtgConnectionIdleWatcher::Post()
	{
	LOG_FUNC

	LOGTEXT(_L8("CUsbOtgConnectionIdleWatcher::Post() - About to call QueueOtgIdPinNotification"));
	iLdd.QueueOtgConnectionNotification(iConnectionIdle, iStatus);
	switch (iConnectionIdle)
		{
		case RUsbOtgDriver::EConnectionIdle:
		case RUsbOtgDriver::EConnectionUnknown:
			RProperty::Set(KUidUsbManCategory,KUsbOtgConnectionIdleProperty,ETrue);
			LOGTEXT2(_L8(">>CUsbOtgConnectionIdleWatcher::Post [iConnectionIdle=%d] - property is set to 1"), iConnectionIdle);
			break;
		case RUsbOtgDriver::EConnectionBusy:
			RProperty::Set(KUidUsbManCategory,KUsbOtgConnectionIdleProperty,EFalse);
			LOGTEXT2(_L8(">>CUsbOtgConnectionIdleWatcher::Post [iConnectionIdle=%d] - property is set to 0"), iConnectionIdle);
			break;
		default:
			LOGTEXT2(_L8(">>CUsbOtgConnectionIdleWatcher::Post [iConnectionIdle=%d] is unrecognized, re-request QueueOtgIdPinNotification"), iConnectionIdle);
			break;
		}
	SetActive();
	}


