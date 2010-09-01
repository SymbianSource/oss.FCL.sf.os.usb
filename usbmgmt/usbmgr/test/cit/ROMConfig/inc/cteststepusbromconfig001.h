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

#ifndef CTESTSTEPUSBROMCONFIG001_H
#define CTESTSTEPUSBROMCONFIG001_H

#include "cteststepusbromconfigbase.h"

class CTestStepUsbRomConfig001 : public CTestStepUsbRomConfigBase
	{
public:
	static CTestStepUsbRomConfig001* New(CTestServer& aParent);
	~CTestStepUsbRomConfig001();
	TVerdict doTestStepL();
	
private:
	CTestStepUsbRomConfig001(CTestServer& aParent);
	};

_LIT(KTestName001, "USB_ROMCONFIG_001"); 
#endif //  CTESTSTEPUSBROMCONFIG001_H

// EOF
