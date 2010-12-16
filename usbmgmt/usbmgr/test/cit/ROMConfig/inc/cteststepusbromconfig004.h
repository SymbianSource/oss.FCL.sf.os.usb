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

#ifndef CTESTSTEPUSBROMCONFIG004_H
#define CTESTSTEPUSBROMCONFIG004_H

#include "cteststepusbromconfigbase.h"

class CTestStepUsbRomConfig004 : public CTestStepUsbRomConfigBase
	{
public:
	static CTestStepUsbRomConfig004* New(CTestServer& aParent);
	~CTestStepUsbRomConfig004();
	TVerdict doTestStepL();
	
private:
	CTestStepUsbRomConfig004(CTestServer& aParent);
	};

_LIT(KTestName004, "USB_ROMCONFIG_004"); 
#endif //  CTESTSTEPUSBROMCONFIG004_H

// EOF