/**
* Copyright (c) 2004-2009 Nokia Corporation and/or its subsidiary(-ies).
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
#if (!defined __CSTOPPERSONALITY_H__)
#define __CSTOPPERSONALITY_H__
#include <test/testexecutestepbase.h>
#include "CUsbTestStepBase.h"

_LIT(KStopPersonality,"StopPersonality");
NONSHARABLE_CLASS(CStopPersonality) : public CUsbTestStepBase 
	{
public:
	CStopPersonality();
	virtual TVerdict doTestStepL();
	};

#endif
