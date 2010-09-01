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

/**
 @file
*/

#include <e32base.h>
#include <usb/acmserver.h>
#include "acmserverimpl.h"
#include <usb/usblogger.h>
#include "acmserverconsts.h"

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ACMSVRCLI");
#endif

/** Panic category for users of RAcmServer. */
#ifdef _DEBUG
_LIT(KAcmSrvPanicCat, "ACMSVR");
#endif

/** Panic codes for users of RAcmServer. */
enum TAcmServerClientPanic
	{
	/** The handle has not been connected. */
	EPanicNotConnected = 0,
	
	/** The handle has already been connected. */
	EPanicAlreadyConnected = 1,

	/** The client has requested to instantiate zero ACM functions, which 
	makes no sense. */
	EPanicCantInstantiateZeroAcms = 2,
	
	/** The client has requested to destroy zero ACM functions, which makes no 
	sense. */
	EPanicCantDestroyZeroAcms = 3,

	/** Close has not been called before destroying the object. */
	EPanicNotClosed = 4,
	};

EXPORT_C RAcmServer::RAcmServer() 
 :	iImpl(NULL)
	{
	LOG_FUNC
	}
	   
EXPORT_C RAcmServer::~RAcmServer()
	{
	LOG_FUNC

	__ASSERT_DEBUG(!iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicNotClosed));
	}

EXPORT_C TInt RAcmServer::Connect()
	{
	LOG_FUNC

	__ASSERT_DEBUG(!iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicAlreadyConnected));
	TRAPD(err, iImpl = CAcmServerImpl::NewL());
	return err;
	}

EXPORT_C void RAcmServer::Close()
	{
	LOG_FUNC

	delete iImpl;
	iImpl = NULL;
	}

EXPORT_C TInt RAcmServer::CreateFunctions(const TUint aNoAcms)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taNoAcms = %d"), aNoAcms);

	__ASSERT_DEBUG(iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicNotConnected));
	__ASSERT_DEBUG(aNoAcms, _USB_PANIC(KAcmSrvPanicCat, EPanicCantInstantiateZeroAcms));
	return iImpl->CreateFunctions(aNoAcms, KDefaultAcmProtocolNum, KControlIfcName, KDataIfcName);
	}
							
EXPORT_C TInt RAcmServer::CreateFunctions(const TUint aNoAcms, const TUint8 aProtocolNum)
	{
	LOG_FUNC
	LOGTEXT3(_L8("\taNoAcms = %d, aProtocolNum = %d"), aNoAcms, aProtocolNum);

	__ASSERT_DEBUG(iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicNotConnected));
	__ASSERT_DEBUG(aNoAcms, _USB_PANIC(KAcmSrvPanicCat, EPanicCantInstantiateZeroAcms));
	return iImpl->CreateFunctions(aNoAcms, aProtocolNum, KControlIfcName, KDataIfcName);
	}

EXPORT_C TInt RAcmServer::CreateFunctions(const TUint aNoAcms, const TUint8 aProtocolNum, const TDesC& aAcmControlIfcName, const TDesC& aAcmDataIfcName)
	{
	LOG_FUNC

	__ASSERT_DEBUG(iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicNotConnected));
	__ASSERT_DEBUG(aNoAcms, _USB_PANIC(KAcmSrvPanicCat, EPanicCantInstantiateZeroAcms));
	return iImpl->CreateFunctions(aNoAcms, aProtocolNum, aAcmControlIfcName, aAcmDataIfcName);
	}

EXPORT_C TInt RAcmServer::DestroyFunctions(const TUint aNoAcms)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taNoAcms = %d"), aNoAcms);

	__ASSERT_DEBUG(iImpl, _USB_PANIC(KAcmSrvPanicCat, EPanicNotConnected));
	__ASSERT_DEBUG(aNoAcms, _USB_PANIC(KAcmSrvPanicCat, EPanicCantDestroyZeroAcms));
	return iImpl->DestroyFunctions(aNoAcms);
	}
