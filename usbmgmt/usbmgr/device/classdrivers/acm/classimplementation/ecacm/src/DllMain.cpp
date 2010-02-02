/*
* Copyright (c) 1997-2009 Nokia Corporation and/or its subsidiary(-ies).
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

#include <e32std.h>
#include <cs_port.h>
#include "AcmPortFactory.h"

extern "C" IMPORT_C CSerial* LibEntryL(void);	

EXPORT_C CSerial* LibEntryL()
/**
 * Lib main entry point
 */
	{	
	return (CAcmPortFactory::NewL());
	}

//
// End of file
