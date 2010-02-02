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

#ifndef CTESTSTEPUSBROMCONFIG003_H
#define CTESTSTEPUSBROMCONFIG003_H

#include "cteststepusbromconfigbase.h"

class CTestStepUsbRomConfig003 : public CTestStepUsbRomConfigBase
	{
public:
	static CTestStepUsbRomConfig003* New(CTestServer& aParent);
	~CTestStepUsbRomConfig003();
	TVerdict doTestStepL();
	
private:
	CTestStepUsbRomConfig003(CTestServer& aParent);
	};

_LIT(KTestName003, "USB_ROMCONFIG_003"); 
#endif //  CTESTSTEPUSBROMCONFIG003_H

// EOF
