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

#include "utils.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

#ifdef __FLOG_ACTIVE
#define LOG Log()
#else
#define LOG
#endif

//*****************************************************************************
// Code relating to the cleanup stack item which 'Remove's a given TUint from 
// an RArray.

TArrayRemove::TArrayRemove(RArray<TUint>& aDeviceIds, TUint aDeviceId)
 :	iDeviceIds(aDeviceIds), 
	iDeviceId(aDeviceId) 
	{
	}

TArrayRemove::~TArrayRemove()
	{
	}

void Remove(TAny* aArrayRemove)
	{
	LOG_STATIC_FUNC_ENTRY

	TArrayRemove* arrayRemove = reinterpret_cast<TArrayRemove*>(aArrayRemove);

	const TUint count = arrayRemove->iDeviceIds.Count();
	for ( TUint ii = 0 ; ii < count ; ++ii )
		{
		if ( arrayRemove->iDeviceIds[ii] == arrayRemove->iDeviceId )
			{
			LOGTEXT(_L8("\tmatching device id"));
			arrayRemove->iDeviceIds.Remove(ii);
			break;
			}
		}
	}

void CleanupRemovePushL(TArrayRemove& aArrayRemove)
	{
	TCleanupItem item(Remove, &aArrayRemove);
	CleanupStack::PushL(item);
	}

//*****************************************************************************
