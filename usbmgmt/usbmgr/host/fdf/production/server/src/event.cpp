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

#include "event.h"
#include <usb/usblogger.h>

#ifdef __FLOG_ACTIVE
_LIT8(KLogComponent, "fdf      ");
#endif

TDeviceEvent::TDeviceEvent()
	{
	LOG_FUNC
	}

TDeviceEvent::~TDeviceEvent()
	{
	LOG_FUNC
	}

#ifdef __FLOG_ACTIVE

void TDeviceEvent::Log() const
	{
	LOGTEXT2(_L8("\tLogging event 0x%08x"), this);
	LOGTEXT2(_L8("\t\tdevice ID = %d"), iInfo.iDeviceId);
	LOGTEXT2(_L8("\t\tevent type = %d"), iInfo.iEventType);

	switch ( iInfo.iEventType )
		{
	case EDeviceAttachment:
		LOGTEXT2(_L8("\t\terror = %d"), iInfo.iError);
		if ( !iInfo.iError )
			{
			LOGTEXT2(_L8("\t\tVID = 0x%04x"), iInfo.iVid);
			LOGTEXT2(_L8("\t\tPID = 0x%04x"), iInfo.iPid);
			}
		break;

	case EDriverLoad:
		LOGTEXT2(_L8("\t\terror = %d"), iInfo.iError);
		LOGTEXT2(_L8("\t\t\tdriver load status = %d"), iInfo.iDriverLoadStatus);
		break;

	case EDeviceDetachment: // No break deliberate.
	default:
		break;
		}
	}

#endif // __FLOG_ACTIVE
