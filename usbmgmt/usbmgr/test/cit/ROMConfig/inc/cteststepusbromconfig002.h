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

#ifndef CTESTSTEPUSBROMCONFIG002_H
#define CTESTSTEPUSBROMCONFIG002_H

#include "cteststepusbromconfigbase.h"

class CTestStepUsbRomConfig002 : public CTestStepUsbRomConfigBase
	{
public:
	static CTestStepUsbRomConfig002* New(CTestServer& aParent);
	~CTestStepUsbRomConfig002();
	TVerdict doTestStepL();
	
private:
	CTestStepUsbRomConfig002(CTestServer& aParent);
	};

_LIT(KTestName002, "USB_ROMCONFIG_002"); 
#endif //  CTESTSTEPUSBROMCONFIG002_H

// EOF
