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
#include "acmserverclient.h"
#include "acmserverconsts.h"
#include <usb/usblogger.h>
#include <usb/acmserver.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "ACMSVRCLI");
#endif

/** Constructor */
RAcmServerClient::RAcmServerClient() 
	{
	LOG_FUNC
	}
	   
/** Destructor */
RAcmServerClient::~RAcmServerClient()
	{
	LOG_FUNC
	}

/**
Getter for the version of the server.
@return Version of the server
*/
TVersion RAcmServerClient::Version() const
	{
	LOG_FUNC

	return TVersion(	KAcmSrvMajorVersionNumber,
						KAcmSrvMinorVersionNumber,
						KAcmSrvBuildNumber
					);
	}

/**
Connect the handle to the server.
Must be called before all other methods (except Version and Close).
@return Symbian error code
*/
TInt RAcmServerClient::Connect()
	{
	LOG_FUNC

	return CreateSession(KAcmServerName, Version(), 1);
	}

TInt RAcmServerClient::CreateFunctions(const TUint aNoAcms, const TUint8 aProtocolNum, const TDesC& aAcmControlIfcName, const TDesC& aAcmDataIfcName)
	{
	LOG_FUNC
	LOGTEXT5(_L("\taNoAcms = %d, aProtocolNum = %d, Control Ifc Name = %S, Data Ifc Name = %S"),
			aNoAcms, aProtocolNum, &aAcmControlIfcName, &aAcmDataIfcName);

	TIpcArgs args;
	args.Set(0, aNoAcms);
	args.Set(1, aProtocolNum);
	args.Set(2, &aAcmControlIfcName);
	args.Set(3, &aAcmDataIfcName);
	return SendReceive(EAcmCreateAcmFunctions, args);
	}

TInt RAcmServerClient::DestroyFunctions(const TUint aNoAcms)
	{
	LOG_FUNC
	LOGTEXT2(_L8("\taNoAcms = %d"), aNoAcms);

	return SendReceive(EAcmDestroyAcmFunctions, TIpcArgs(aNoAcms));
	}
