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

#include <e32base.h>
#include <usb/usblogger.h>
#include "usbhoststack.h"
#include "fdfapi.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "usbhstcli");
#endif

/**
Starts the server process.
*/
static TInt StartServer()
	{
	LOG_STATIC_FUNC_ENTRY

	const TUidType serverUid(KNullUid, KNullUid, KUsbFdfUid);

	//
	// EPOC and EKA2 is easy, we just create a new server process. Simultaneous
	// launching of two such processes should be detected when the second one
	// attempts to create the server object, failing with KErrAlreadyExists.
	//
	RProcess server;
	TInt err = server.Create(KUsbFdfImg, KNullDesC, serverUid);
	LOGTEXT2(_L8("\terr = %d"), err);

	if ( err != KErrNone )
		{
		return err;
		}

	TRequestStatus stat;
	server.Rendezvous(stat);

	if ( stat != KRequestPending )
		{
		LOGTEXT(_L8("\taborting startup"));
		server.Kill(0); 	// abort startup
		}
	else
		{
		LOGTEXT(_L8("\tresuming"));
		server.Resume();	// logon OK - start the server
		}

	User::WaitForRequest(stat); 	// wait for start or death

	// we can't use the 'exit reason' if the server panicked as this
	// is the panic 'reason' and may be '0' which cannot be distinguished
	// from KErrNone
	LOGTEXT2(_L8("\tstat.Int = %d"), stat.Int());
	err = (server.ExitType() == EExitPanic) ? KErrServerTerminated : stat.Int();

	server.Close();

	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

EXPORT_C RUsbHostStack::RUsbHostStack()
	// these are all arbitrary initialisations
 :	iDeviceEventPckg(TDeviceEventInformation()),
	iDevmonEventPckg(0)
	{
	LOGTEXT(_L8("*** Search on '***USB HOST STACK' to find device events."));

	LOG_LINE
	LOG_FUNC
	}

EXPORT_C TVersion RUsbHostStack::Version() const
	{
	LOG_LINE
	LOG_FUNC

	return(TVersion(	KUsbFdfSrvMajorVersionNumber,
						KUsbFdfSrvMinorVersionNumber,
						KUsbFdfSrvBuildNumber
					)
			);
	}

EXPORT_C TInt RUsbHostStack::Connect()
	{
	LOG_LINE
	LOG_FUNC;

	TInt err = DoConnect();

	LOGTEXT2(_L8("\terr = %d"), err);
	return err;
	}

/**
Connects the session, starting the server if necessary
@return Error.
*/
TInt RUsbHostStack::DoConnect()
	{
	LOG_FUNC

	TInt retry = 2;

	FOREVER
		{
		// Use message slots from the global pool.
		TInt err = CreateSession(KUsbFdfServerName, Version(), -1);
		LOGTEXT2(_L8("\terr = %d"), err);

		if ((err != KErrNotFound) && (err != KErrServerTerminated))
			{
			LOGTEXT(_L8("\treturning after CreateSession"));
			return err;
			}

		if (--retry == 0)
			{
			LOGTEXT(_L8("\treturning after running out of retries"));
			return err;
			}

		err = StartServer();
		LOGTEXT2(_L8("\terr = %d"), err);

		if ((err != KErrNone) && (err != KErrAlreadyExists))
			{
			LOGTEXT(_L8("\treturning after StartServer"));
			return err;
			}
		}
	}

EXPORT_C TInt RUsbHostStack::EnableDriverLoading()
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvEnableDriverLoading);
	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

EXPORT_C void RUsbHostStack::DisableDriverLoading()
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvDisableDriverLoading);
	LOGTEXT2(_L8("\tret = %d"), ret);
	(void)ret;
	}

EXPORT_C void RUsbHostStack::NotifyDeviceEvent(TRequestStatus& aStat, TDeviceEventInformation& aDeviceEventInformation)
	{
	LOG_LINE
	LOG_FUNC

	TIpcArgs args;
	iDeviceEventPckg.Set((TUint8*)&aDeviceEventInformation, sizeof(TDeviceEventInformation), sizeof(TDeviceEventInformation));
	args.Set(0, &iDeviceEventPckg);

	SendReceive(EUsbFdfSrvNotifyDeviceEvent, args, aStat);
	}

EXPORT_C void RUsbHostStack::NotifyDeviceEventCancel()
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvNotifyDeviceEventCancel);
	LOGTEXT2(_L8("\tret = %d"), ret);
	(void)ret;
	}

EXPORT_C void RUsbHostStack::NotifyDevmonEvent(TRequestStatus& aStat, TInt& aEvent)
	{
	LOG_LINE
	LOG_FUNC

	TIpcArgs args;
	iDevmonEventPckg.Set((TUint8*)&aEvent, sizeof(TInt), sizeof(TInt));
	args.Set(0, &iDevmonEventPckg);

	SendReceive(EUsbFdfSrvNotifyDevmonEvent, args, aStat);
	}

EXPORT_C void RUsbHostStack::NotifyDevmonEventCancel()
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvNotifyDevmonEventCancel);
	LOGTEXT2(_L8("\tret = %d"), ret);
	(void)ret;
	}

EXPORT_C TInt RUsbHostStack::GetSupportedLanguages(TUint aDeviceId, RArray<TUint>& aLangIds)
	{
	LOG_LINE
	LOG_FUNC

	aLangIds.Reset();

	TUint singleLangIdOrNumLangs = 0;
	TPckg<TUint> singleLangIdOrNumLangsBuf(singleLangIdOrNumLangs);
	TInt ret = SendReceive(EUsbFdfSrvGetSingleSupportedLanguageOrNumberOfSupportedLanguages, TIpcArgs(aDeviceId, &singleLangIdOrNumLangsBuf));
	LOGTEXT2(_L8("\tsingleLangIdOrNumLangs = %d"), singleLangIdOrNumLangs);
	switch ( ret )
		{
	case KErrNotFound:
		LOGTEXT2(_L8("\tThere is no language available or the wrong device id %d was supplied"),aDeviceId);
		ret = KErrNotFound;
		break;

	case KErrNone:
		// The buffer is now either empty or contains the single supported language ID
		ret = CopyLangIdsToArray(aLangIds, singleLangIdOrNumLangsBuf);
		break;

	case KErrTooBig:
		{
		// The buffer now contains the number of supported languages (not 0 or 1).
		// Lang IDs are TUints.
		RBuf8 buf;
		ret = buf.Create(singleLangIdOrNumLangs * sizeof(TUint));
		if ( ret == KErrNone )
			{
			ret = SendReceive(EUsbFdfSrvGetSupportedLanguages, TIpcArgs(aDeviceId, &buf));
			if ( ret == KErrNone )
				{
				ret = CopyLangIdsToArray(aLangIds, buf);
				}
			buf.Close();
			}
		}
		break;

	default:
		// Regular failure.
		break;
		}

	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

TInt RUsbHostStack::CopyLangIdsToArray(RArray<TUint>& aLangIds, const TDesC8& aBuffer)
	{
	LOG_FUNC

	ASSERT(!(aBuffer.Size() % 4));
	const TUint numLangs = aBuffer.Size() / 4;
	LOGTEXT2(_L8("\tnumLangs = %d"), numLangs);

	TInt ret = KErrNone;
	const TUint* ptr = reinterpret_cast<const TUint*>(aBuffer.Ptr());
	for ( TUint ii = 0 ; ii < numLangs ; ++ii )
		{
		ret = aLangIds.Append(*ptr++); // increments by sizeof(TUint)
		if ( ret )
			{
			aLangIds.Reset();
			break;
			}
		}

	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

EXPORT_C TInt RUsbHostStack::GetManufacturerStringDescriptor(TUint aDeviceId, TUint aLangId, TName& aString)
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvGetManufacturerStringDescriptor, TIpcArgs(aDeviceId, aLangId, &aString));
#ifdef __FLOG_ACTIVE
	if ( !ret )
		{
		LOGTEXT2(_L("\taString = \"%S\""), &aString);
		}
#endif
	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

EXPORT_C TInt RUsbHostStack::GetProductStringDescriptor(TUint aDeviceId, TUint aLangId, TName& aString)
	{
	LOG_LINE
	LOG_FUNC

	TInt ret = SendReceive(EUsbFdfSrvGetProductStringDescriptor, TIpcArgs(aDeviceId, aLangId, &aString));
#ifdef __FLOG_ACTIVE
	if ( !ret )
		{
		LOGTEXT2(_L("\taString = \"%S\""), &aString);
		}
#endif
	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

EXPORT_C TInt RUsbHostStack::GetOtgDescriptor(TUint aDeviceId, TOtgDescriptor& aDescriptor)
	{
	LOG_LINE
	LOG_FUNC

	TPckg<TOtgDescriptor> otgDescriptorPckg(aDescriptor);
	
	TIpcArgs args;
	args.Set(0, aDeviceId);
	args.Set(1, &otgDescriptorPckg);

	TInt ret = SendReceive(EUsbFdfSrvGetOtgDescriptor, args);
#ifdef __FLOG_ACTIVE
	if ( !ret )
		{
		LOGTEXT2(_L("\taDescriptor.iDeviceId = %d"), aDescriptor.iDeviceId);
		LOGTEXT2(_L("\taDescriptor.iAttributes = %d"), aDescriptor.iAttributes);
		}
#endif
	LOGTEXT2(_L8("\tret = %d"), ret);
	return ret;
	}

EXPORT_C TInt RUsbHostStack::__DbgFailNext(TInt aCount)
	{
#ifdef _DEBUG
	return SendReceive(EUsbFdfSrvDbgFailNext, TIpcArgs(aCount));
#else
	(void)aCount;
	return KErrNone;
#endif
	}

EXPORT_C TInt RUsbHostStack::__DbgAlloc()
	{
#ifdef _DEBUG
	return SendReceive(EUsbFdfSrvDbgAlloc);
#else
	return KErrNone;
#endif
	}
