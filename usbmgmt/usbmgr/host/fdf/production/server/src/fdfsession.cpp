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

/**
 @file
 @internalComponent
*/

#include "fdfsession.h"
#include "fdfserver.h"
#include <usb/usblogger.h>
#include "utils.h"
#include <ecom/ecom.h>
#include "fdfapi.h"
#include "fdf.h"
#include "event.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef _DEBUG
PANICCATEGORY("fdfsession");
#endif

CFdfSession::CFdfSession(CFdf& aFdf, CFdfServer& aServer)
 :	iFdf(aFdf),
	iServer(aServer)
	{
	LOG_FUNC
	}

CFdfSession::~CFdfSession()
	{
	LOG_LINE
	LOG_FUNC;

	iServer.SessionClosed();
	}

void CFdfSession::ServiceL(const RMessage2& aMessage)
	{
	LOG_LINE
	LOG_FUNC;
	LOGTEXT2(_L8("\taMessage.Function() = %d"), aMessage.Function());

	// Switch on the IPC number and call a 'message handler'. Message handlers
	// complete aMessage (either with Complete or Panic), or make a note of
	// the message for later asynchronous completion.
	// Message handlers should not leave- the server does not have an Error
	// function.

	switch ( aMessage.Function() )
		{
	case EUsbFdfSrvEnableDriverLoading:
		EnableDriverLoading(aMessage);
		// This is a sync API- check that the message has been completed.
		// (NB We don't check the converse for async APIs because the message
		// may have been panicked synchronously.)
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvDisableDriverLoading:
		DisableDriverLoading(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvNotifyDeviceEvent:
		NotifyDeviceEvent(aMessage);
		break;

	case EUsbFdfSrvNotifyDeviceEventCancel:
		NotifyDeviceEventCancel(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvNotifyDevmonEvent:
		NotifyDevmonEvent(aMessage);
		break;

	case EUsbFdfSrvNotifyDevmonEventCancel:
		NotifyDevmonEventCancel(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvGetSingleSupportedLanguageOrNumberOfSupportedLanguages:
		GetSingleSupportedLanguageOrNumberOfSupportedLanguages(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvGetSupportedLanguages:
		GetSupportedLanguages(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvGetManufacturerStringDescriptor:
		GetManufacturerStringDescriptor(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	case EUsbFdfSrvGetProductStringDescriptor:
		GetProductStringDescriptor(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;
		
	case EUsbFdfSrvGetOtgDescriptor:
		GetOtgDeviceDescriptor(aMessage);
		ASSERT_DEBUG(aMessage.Handle() == 0);
		break;

	// Heap failure testing APIs.
	case EUsbFdfSrvDbgFailNext:
#ifdef _DEBUG
		{
		LOGTEXT2(_L8("\tfail next (simulating failure after %d allocation(s))"), aMessage.Int0());
		if ( aMessage.Int0() == 0 )
			{
			__UHEAP_RESET;
			}
		else
			{
			__UHEAP_FAILNEXT(aMessage.Int0());
			}
		}
#endif // _DEBUG
		CompleteClient(aMessage, KErrNone);
		break;

	case EUsbFdfSrvDbgAlloc:
		{
		TInt err = KErrNone;
#ifdef _DEBUG
		LOGTEXT(_L8("\tallocate on the heap"));
		TInt* x = NULL;
		TRAP(err, x = new(ELeave) TInt);
		delete x;

#endif // _DEBUG
		CompleteClient(aMessage, err);
		}
		break;

	default:
		PANIC_MSG(aMessage, KUsbFdfServerName, EBadIpc);
		break;
		}
	}

void CFdfSession::CompleteClient(const RMessage2& aMessage, TInt aError)
	{
	LOGTEXT2(_L8("\tcompleting client message with %d"), aError);
	aMessage.Complete(aError);
	}

void CFdfSession::EnableDriverLoading(const RMessage2& aMessage)
	{
	LOG_FUNC

	iFdf.EnableDriverLoading();

	CompleteClient(aMessage, KErrNone);
	}

void CFdfSession::DisableDriverLoading(const RMessage2& aMessage)
	{
	LOG_FUNC

	iFdf.DisableDriverLoading();

	CompleteClient(aMessage, KErrNone);
	}

TBool CFdfSession::NotifyDeviceEventOutstanding() const
	{
	const TBool ret = ( iNotifyDeviceEventMsg.Handle() != 0 );
	LOGTEXT2(_L("CFdfSession::NotifyDeviceEventOutstanding returning %d"), ret);
	return ret;
	}

void CFdfSession::NotifyDeviceEvent(const RMessage2& aMessage)
	{
	LOG_FUNC

	if ( iNotifyDeviceEventMsg.Handle() )
		{
		PANIC_MSG(iNotifyDeviceEventMsg, KUsbFdfServerName, ENotifyDeviceEventAlreadyOutstanding);
		}
	else
		{
		iNotifyDeviceEventMsg = aMessage;
		TDeviceEvent event;
		if ( iFdf.GetDeviceEvent(event) )
			{
			CompleteDeviceEventNotification(event);
			}
		}
	}

void CFdfSession::NotifyDeviceEventCancel(const RMessage2& aMessage)
	{
	LOG_FUNC

	if ( iNotifyDeviceEventMsg.Handle() )
		{
		CompleteClient(iNotifyDeviceEventMsg, KErrCancel);
		}
	CompleteClient(aMessage, KErrNone);
	}

void CFdfSession::DeviceEvent(const TDeviceEvent& aEvent)
	{
	LOG_FUNC

	// This function should only be called if there is a request outstanding.
	ASSERT_DEBUG(iNotifyDeviceEventMsg.Handle());

	CompleteDeviceEventNotification(aEvent);
	}

void CFdfSession::CompleteDeviceEventNotification(const TDeviceEvent& aEvent)
	{
	LOG_FUNC

	TRAPD(err, CompleteDeviceEventNotificationL(aEvent));
	if ( err )
		{
		PANIC_MSG(iNotifyDeviceEventMsg, KUsbFdfServerName, EBadNotifyDeviceEventData);
		}
	}

void CFdfSession::CompleteDeviceEventNotificationL(const TDeviceEvent& aEvent)
	{
	LOG_FUNC

	// iNotifyDeviceEventMsg has one IPC arg: a TDeviceEventInformation

	ASSERT_DEBUG(iNotifyDeviceEventMsg.Handle());

	TPckg<TDeviceEventInformation> info(aEvent.iInfo);
	iNotifyDeviceEventMsg.WriteL(0, info);

	CompleteClient(iNotifyDeviceEventMsg, KErrNone);
	}

TBool CFdfSession::NotifyDevmonEventOutstanding() const
	{
	const TBool ret = ( iNotifyDevmonEventMsg.Handle() != 0 );
	LOGTEXT2(_L("CFdfSession::NotifyDevmonEventOutstanding returning %d"), ret);
	return ret;
	}

void CFdfSession::NotifyDevmonEvent(const RMessage2& aMessage)
	{
	LOG_FUNC

	if ( iNotifyDevmonEventMsg.Handle() )
		{
		PANIC_MSG(iNotifyDevmonEventMsg, KUsbFdfServerName, ENotifyDevmonEventAlreadyOutstanding);
		}
	else
		{
		iNotifyDevmonEventMsg = aMessage;
		TInt event;
		if ( iFdf.GetDevmonEvent(event) )
			{
			CompleteDevmonEventNotification(event);
			}
		}
	}

void CFdfSession::NotifyDevmonEventCancel(const RMessage2& aMessage)
	{
	LOG_FUNC

	if ( iNotifyDevmonEventMsg.Handle() )
		{
		CompleteClient(iNotifyDevmonEventMsg, KErrCancel);
		}
	CompleteClient(aMessage, KErrNone);
	}

void CFdfSession::DevmonEvent(TInt aError)
	{
	LOG_FUNC

	// This function should only be called if there is a request outstanding.
	ASSERT_DEBUG(iNotifyDevmonEventMsg.Handle());

	CompleteDevmonEventNotification(aError);
	}

void CFdfSession::CompleteDevmonEventNotification(TInt aError)
	{
	LOG_FUNC

	TRAPD(err, CompleteDevmonEventNotificationL(aError));
	if ( err )
		{
		PANIC_MSG(iNotifyDevmonEventMsg, KUsbFdfServerName, EBadNotifyDevmonEventData);
		}
	}

void CFdfSession::CompleteDevmonEventNotificationL(TInt aEvent)
	{
	LOG_FUNC

	// iNotifyDevmonEventMsg has the following IPC args:
	// 0- TInt& aError

	ASSERT_DEBUG(iNotifyDevmonEventMsg.Handle());

	TPckg<TInt> event(aEvent);
	iNotifyDevmonEventMsg.WriteL(0, event);

	CompleteClient(iNotifyDevmonEventMsg, KErrNone);
	}

void CFdfSession::GetSingleSupportedLanguageOrNumberOfSupportedLanguages(const RMessage2& aMessage)
	{
	LOG_FUNC

	// To save IPC operations between client and server, we make use of the
	// fact that the majority of devices only support a single language.
	// The client is expected to have a buffer big enough to hold a single
	// TUint.
	// If the device supports 0 languages, the buffer is left empty and the
	// request is completed with KErrNotFound.
	// If the device supports 1 language, the language ID is put in the buffer
	// and the request is completed with KErrNone.
	// If the device supports more than 1 language, the number of languages is
	// put in the buffer, and the request is completed with
	// KErrTooBig. The client then allocates a buffer big enough to hold the
	// supported languages and uses EUsbFdfSrvGetSupportedLanguages to get
	// them all.
	TRAPD(err, GetSingleSupportedLanguageOrNumberOfSupportedLanguagesL(aMessage));
	CompleteClient(aMessage, err);
	}

void CFdfSession::GetSingleSupportedLanguageOrNumberOfSupportedLanguagesL(const RMessage2& aMessage)
	{
	LOG_FUNC

	const TUint deviceId = aMessage.Int0();
	LOGTEXT2(_L8("\tdeviceId = %d"), deviceId);
	const RArray<TUint>& langIds = iFdf.GetSupportedLanguagesL(deviceId);
	const TUint count = langIds.Count();
	LOGTEXT2(_L8("\tcount = %d"), count);
	switch ( count )
		{
	case 0:
		// Nothing to write to the client's address space, complete with
		LEAVEL(KErrNotFound);
		break;

	case 1:
		{
		// Write the single supported language to the client, complete with
		// KErrNone (or error of course, if their buffer isn't big enough).
		TPckg<TUint> buf(langIds[0]);
		LEAVEIFERRORL(aMessage.Write(1, buf));
		}
		break;

	default:
		{
		// Write the number of supported languages to the client, complete
		// with KErrTooBig (or error if their buffer wasn't big enough). NB
		// This is the point at which this mechanism depends on
		// RMessagePtr2::WriteL itself not leaving with KErrTooBig!
		TPckg<TUint> buf(count);
		LEAVEIFERRORL(aMessage.Write(1, buf));
		LEAVEL(KErrTooBig);
		}
		break;
		}
	}

void CFdfSession::GetSupportedLanguages(const RMessage2& aMessage)
	{
	LOG_FUNC

	TRAPD(err, GetSupportedLanguagesL(aMessage));
	CompleteClient(aMessage, err);
	}

void CFdfSession::GetSupportedLanguagesL(const RMessage2& aMessage)
	{
	LOG_FUNC

	const TUint deviceId = aMessage.Int0();
	LOGTEXT2(_L8("\tdeviceId = %d"), deviceId);
	const RArray<TUint>& langIds = iFdf.GetSupportedLanguagesL(deviceId);

	const TUint count = langIds.Count();
	LOGTEXT2(_L8("\tcount = %d"), count);
	RBuf8 buf;
	buf.CreateL(count * sizeof(TUint));
	CleanupClosePushL(buf);
	for ( TUint ii = 0 ; ii < count ; ++ii )
		{
		buf.Append((TUint8*)&(langIds[ii]), sizeof(TUint));
		}

	// Write back to the client.
	LEAVEIFERRORL(aMessage.Write(1, buf));
	CleanupStack::PopAndDestroy(&buf);
	}

void CFdfSession::GetManufacturerStringDescriptor(const RMessage2& aMessage)
	{
	LOG_FUNC

	GetStringDescriptor(aMessage, EManufacturer);
	}

void CFdfSession::GetProductStringDescriptor(const RMessage2& aMessage)
	{
	LOG_FUNC

	GetStringDescriptor(aMessage, EProduct);
	}

void CFdfSession::GetStringDescriptor(const RMessage2& aMessage, TStringType aStringType)
	{
	LOG_FUNC

	TRAPD(err, GetStringDescriptorL(aMessage, aStringType));
	CompleteClient(aMessage, err);
	}

void CFdfSession::GetStringDescriptorL(const RMessage2& aMessage, TStringType aStringType)
	{
	LOG_FUNC

	ASSERT_DEBUG(aStringType == EManufacturer || aStringType == EProduct);

	TName string;
	const TUint deviceId = aMessage.Int0();
	const TUint langId = aMessage.Int1();
	if ( aStringType == EManufacturer )
		{
		iFdf.GetManufacturerStringDescriptorL(deviceId, langId, string);
		}
	else
		{
		iFdf.GetProductStringDescriptorL(deviceId, langId, string);
		}
	LOGTEXT2(_L("\tstring = \"%S\""), &string);
	LEAVEIFERRORL(aMessage.Write(2, string));
	}

void CFdfSession::GetOtgDeviceDescriptor(const RMessage2& aMessage)
	{
	LOG_FUNC
	
	TOtgDescriptor otgDesc;
	const TUint deviceId = aMessage.Int0();
	TRAPD(err, iFdf.GetOtgDeviceDescriptorL(deviceId, otgDesc));	
	if (KErrNone == err)
		{
		TPckg<TOtgDescriptor> buf(otgDesc);
		err = aMessage.Write(1, buf);
		}
	CompleteClient(aMessage, err);
	}
