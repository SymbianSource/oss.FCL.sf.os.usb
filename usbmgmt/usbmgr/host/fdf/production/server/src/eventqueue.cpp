/*
* Copyright (c) 2007-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include "eventqueue.h"
#include <usb/usblogger.h>
#include "fdf.h"
#include "fdfsession.h"
#include "utils.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef __FLOG_ACTIVE
#define LOG Log()
#else
#define LOG
#endif

#ifdef _DEBUG
PANICCATEGORY("eventq");
#endif

class CFdfSession;

CEventQueue* CEventQueue::NewL(CFdf& aFdf)
	{
	LOG_STATIC_FUNC_ENTRY

	CEventQueue* self = new(ELeave) CEventQueue(aFdf);
	return self;
	}

CEventQueue::CEventQueue(CFdf& aFdf)
:	iFdf(aFdf),
	iDeviceEvents(_FOFF(TDeviceEvent, iLink))
	{
	LOG_FUNC
	}

CEventQueue::~CEventQueue()
	{
	LOG_FUNC

	// There will be things left on the queue at this time if USBMAN shuts us
	// down without having picked up everything that was on the queue.
	// This is valid behaviour and these events need destroying now.
	TSglQueIter<TDeviceEvent> iter(iDeviceEvents);
	iter.SetToFirst();
	TDeviceEvent* event;
	while ( ( event = iter++ ) != NULL )
		{
		delete event;
		}
	}

// Increments the count of failed attachments.
void CEventQueue::AttachmentFailure(TInt aError)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taError = %d"), aError);
	LOG;

	TUint index = 0;
	switch ( aError )
		{
	case KErrUsbSetAddrFailed:
		index = KSetAddrFailed;
		break;
	case KErrUsbNoPower:
		index = KNoPower;
		break;
	case KErrBadPower:
		index = KBadPower;
		break;
	case KErrUsbIOError:
		index = KIOError;
		break;
	case KErrUsbTimeout:
		index = KTimeout;
		break;
	case KErrUsbStalled:
		index = KStalled;
		break;
	case KErrNoMemory:
		index = KNoMemory;
		break;
	case KErrUsbConfigurationHasNoInterfaces:
		index = KConfigurationHasNoInterfaces;
		break;
	case KErrUsbInterfaceCountMismatch:
		index = KInterfaceCountMismatch;
		break;
	case KErrUsbDuplicateInterfaceNumbers:
		index = KDuplicateInterfaceNumbers;
		break;
	case KErrBadHandle:
		index = KBadHandle;
		break;

	default:
		// we must deal with every error we are ever given
		LOGTEXT2(_L8("\tFDF did not expect this error %d as a fail attachment"), aError);
		index = KAttachmentFailureGeneralError;
		break;
		}
	++(iAttachmentFailureCount[index]);

	PokeSession();
	LOG;
	}

// Called to add an event to the tail of the queue.
// Takes ownership of aEvent.
void CEventQueue::AddDeviceEvent(TDeviceEvent& aEvent)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\t&aEvent = 0x%08x"), &aEvent);
	LOG;

	iDeviceEvents.AddLast(aEvent);

	PokeSession();
	LOG;
	}

// Poke the session object (if it exists) to complete any outstanding event
// notifications it has.
// It only makes sense to call this function if there's some event to give up.
void CEventQueue::PokeSession()
	{
	LOG_FUNC

	// If the session exists, and has a notification outstanding, give them
	// the head event.
	CFdfSession* sess = iFdf.Session();
	if ( sess )
		{
		if ( sess->NotifyDevmonEventOutstanding() )
			{
			TInt event;
			if ( GetDevmonEvent(event) )
				{
				sess->DevmonEvent(event);
				}
			}
		if ( sess->NotifyDeviceEventOutstanding() )
			{
			TDeviceEvent event;
			if ( GetDeviceEvent(event) )
				{
				sess->DeviceEvent(event);
				}
			}
		}
	}

// This is called to get a device event. Attachment failures are given up
// before actual queued events.
// If ETrue is returned, the queued event is destroyed and a copy is delivered
// in aEvent.
TBool CEventQueue::GetDeviceEvent(TDeviceEvent& aEvent)
	{
	LOG_FUNC
	LOG;

	TBool ret = EFalse;

	// We need to see if any attachment failures have been collected by us. If
	// there have, report exactly one of them to the client.
	for ( TUint ii = 0 ; ii < KNumberOfAttachmentFailureTypes ; ++ii )
		{
		TUint& errorCount = iAttachmentFailureCount[ii];
		if ( errorCount )
			{
			--errorCount;
			switch ( ii )
				{
			case KSetAddrFailed:
				aEvent.iInfo.iError = KErrUsbSetAddrFailed;
				break;
			case KNoPower:
				aEvent.iInfo.iError = KErrUsbNoPower;
				break;
			case KBadPower:
				aEvent.iInfo.iError = KErrBadPower;
				break;
			case KIOError:
				aEvent.iInfo.iError = KErrUsbIOError;
				break;
			case KTimeout:
				aEvent.iInfo.iError = KErrUsbTimeout;
				break;
			case KStalled:
				aEvent.iInfo.iError = KErrUsbStalled;
				break;
			case KNoMemory:
				aEvent.iInfo.iError = KErrNoMemory;
				break;
			case KConfigurationHasNoInterfaces:
				aEvent.iInfo.iError = KErrUsbConfigurationHasNoInterfaces;
				break;
			case KInterfaceCountMismatch:
				aEvent.iInfo.iError = KErrUsbInterfaceCountMismatch;
				break;
			case KDuplicateInterfaceNumbers:
				aEvent.iInfo.iError = KErrUsbDuplicateInterfaceNumbers;
				break;
			case KBadHandle:
				aEvent.iInfo.iError = KErrBadHandle;
				break;
			case KAttachmentFailureGeneralError:
				aEvent.iInfo.iError = KErrUsbAttachmentFailureGeneralError;
				break;

			case KNumberOfAttachmentFailureTypes:
			default:
				// this switch should deal with every error type we store
				ASSERT_DEBUG(0);

				}

			ret = ETrue;
			aEvent.iInfo.iEventType = EDeviceAttachment;
			LOGTEXT2(_L8("\treturning attachment failure event (code %d)"), aEvent.iInfo.iError);
			// Only give the client one error at a time.
			break;
			}
		}

	if ( !ret && !iDeviceEvents.IsEmpty() )
		{
		TDeviceEvent* const event = iDeviceEvents.First();
		LOGTEXT2(_L8("\tevent = 0x%08x"), event);
		iDeviceEvents.Remove(*event);
		(void)Mem::Copy(&aEvent, event, sizeof(TDeviceEvent));
		delete event;
		ret = ETrue;
		}

	LOG;
	LOGTEXT2(_L8("\treturning %d"), ret);
	return ret;
	}

TBool CEventQueue::GetDevmonEvent(TInt& aEvent)
	{
	LOG_FUNC
	LOG;

	TBool ret = EFalse;

	for ( TUint ii = 0 ; ii < KNumberOfDevmonEventTypes ; ++ii )
		{
		TUint& eventCount = iDevmonEventCount[ii];
		if ( eventCount )
			{
			--eventCount;
			switch ( ii )
				{

			case KUsbDeviceRejected:
				aEvent = KErrUsbDeviceRejected;
				break;
			case KUsbDeviceFailed:
				aEvent = KErrUsbDeviceFailed;
				break;
			case KUsbBadDevice:
				aEvent = KErrUsbBadDevice;
				break;
			case KUsbBadHubPosition:
				aEvent = KErrUsbBadHubPosition;
				break;
			case KUsbBadHub:
				aEvent = KErrUsbBadHub;
				break;
			case KUsbEventOverflow:
				aEvent = KErrUsbEventOverflow;
				break;
			case KUsbBadDeviceAttached:
			    aEvent = KErrUsbBadDeviceAttached;
			    break;
			case KUsbBadDeviceDetached:
			    aEvent = KEventUsbBadDeviceDetached;
			    break;
			case KNumberOfDevmonEventTypes:
			default:
				LOGTEXT2(_L8("\tUnexpected devmon error, not handled properly %d"), ii);
				ASSERT_DEBUG(0);
				aEvent = KErrUsbDeviceRejected;
				// this switch should deal with every error type we store
				}

			ret = ETrue;
			// Only give the client one error at a time.
			break;
			}
		}

	LOG;
	LOGTEXT2(_L8("\treturning %d"), ret);
	return ret;
	}

void CEventQueue::AddDevmonEvent(TInt aEvent)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taEvent = %d"), aEvent);

	// Increment the relevant count.
	TInt index = 0;
	switch ( aEvent )
		{

		case KErrUsbDeviceRejected:
			index = KUsbDeviceRejected;
			break;
		case KErrUsbDeviceFailed:
			index = KUsbDeviceFailed;
			break;
		case KErrUsbBadDevice:
			index = KUsbBadDevice;
			break;
		case KErrUsbBadHubPosition:
			index = KUsbBadHubPosition;
			break;
		case KErrUsbBadHub:
			index = KUsbBadHub;
			break;
		case KErrUsbEventOverflow:
			index = KUsbEventOverflow;
			break;
        case KErrUsbBadDeviceAttached:
            index = KUsbBadDeviceAttached;
            break;
        case KEventUsbBadDeviceDetached:
            index = KUsbBadDeviceDetached;
            break;			

		default:
			LOGTEXT2(_L8("\tUnexpected devmon error, not handled properly %d"), aEvent);
			ASSERT_DEBUG(0);
			// this switch should deal with every type of event we ever receive from devmon
			}

	TUint& eventCount = iDevmonEventCount[index];
	ASSERT_DEBUG(eventCount < KMaxTUint);
	++eventCount;
	PokeSession();
	}

#ifdef __FLOG_ACTIVE

void CEventQueue::Log()
	{
	LOG_FUNC

	for ( TUint ii = 0 ; ii < KNumberOfAttachmentFailureTypes ; ++ii )
		{
		const TInt& errorCount = iAttachmentFailureCount[ii];
		if ( errorCount )
			{
			LOGTEXT3(_L8("\tNumber of attachment failures of type %d is %d"), ii, errorCount);
			}
		}

	for ( TUint ii = 0 ; ii < KNumberOfDevmonEventTypes ; ++ii )
		{
		const TInt& eventCount = iDevmonEventCount[ii];
		if ( eventCount )
			{
			LOGTEXT3(_L8("\tNumber of devmon events of type %d is %d"), ii, eventCount);
			}
		}

	TUint pos = 0;
	TSglQueIter<TDeviceEvent> iter(iDeviceEvents);
	iter.SetToFirst();
	TDeviceEvent* event;
	while ( ( event = iter++ ) != NULL )
		{
		LOGTEXT2(_L8("\tLogging event at position %d"), pos);
		event->Log();
		++pos;
		}
	}

#endif // __FLOG_ACTIVE

